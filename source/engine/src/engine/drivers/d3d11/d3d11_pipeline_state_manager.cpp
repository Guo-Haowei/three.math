#include "d3d11_pipeline_state_manager.h"

#include <d3dcompiler.h>

#include <fstream>

#include "core/framework/asset_manager.h"
#include "drivers/d3d11/d3d11_graphics_manager.h"
#include "drivers/d3d11/d3d11_helpers.h"
#include "drivers/d3d_common/d3d_common.h"
#include "drivers/d3d_common/d3d_convert.h"

namespace my {

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

class D3DIncludeHandler : public ID3DInclude {
public:
    STDMETHOD(Open)
    (D3D_INCLUDE_TYPE, LPCSTR p_file, LPCVOID, LPCVOID* p_out_data, UINT* p_bytes) override {
        // @TODO: fix search
        FilePath path = FilePath{ ROOT_FOLDER } / "source/shader/" / p_file;

        auto source_binary = AssetManager::GetSingleton().LoadFileSync(path);
        if (!source_binary || source_binary->buffer.empty()) {
            LOG_ERROR("failed to read file '{}'", path.String());
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        *p_out_data = source_binary->buffer.data();
        *p_bytes = (UINT)source_binary->buffer.size();
        return S_OK;
    }

    STDMETHOD(Close)
    (LPCVOID) override {
        return S_OK;
    }
};

static auto CompileShader(std::string_view p_path, const char* p_target, const D3D_SHADER_MACRO* p_defines) -> std::expected<ComPtr<ID3DBlob>, std::string> {
    fs::path fullpath = fs::path{ ROOT_FOLDER } / "source" / "shader" / "hlsl" / (std::string(p_path) + ".hlsl");
    std::string fullpath_str = fullpath.string();

    std::wstring path{ fullpath_str.begin(), fullpath_str.end() };
    ComPtr<ID3DBlob> error;
    ComPtr<ID3DBlob> source;

    D3DIncludeHandler include_handler;

    uint32_t flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        path.c_str(),
        p_defines,
        &include_handler,
        "main",
        p_target,
        flags,
        0,
        source.GetAddressOf(),
        error.GetAddressOf());

    if (FAILED(hr)) {
        if (error != nullptr) {
            return std::unexpected(std::string((const char*)error->GetBufferPointer()));
        } else {
            return std::unexpected("Unkown error");
        }
    }

    return source;
}

std::shared_ptr<PipelineState> D3d11PipelineStateManager::CreateInternal(const PipelineCreateInfo& p_info) {
    auto graphics_manager = reinterpret_cast<D3d11GraphicsManager*>(GraphicsManager::GetSingletonPtr());
    // @TODO: try not to expose device
    auto& device = graphics_manager->GetD3dDevice();
    DEV_ASSERT(device);

    if (!device) {
        return nullptr;
    }

    std::vector<D3D_SHADER_MACRO> defines;
    for (const auto& define : p_info.defines) {
        defines.push_back({ define.name, define.value });
    }
    defines.push_back({ "HLSL_LANG", "1" });
    defines.push_back({ nullptr, nullptr });

    if (!p_info.cs.empty()) {
        return CreateComputePipeline(device, p_info, defines);
    }

    return CreateGraphicsPipeline(device, p_info, defines);
}

std::shared_ptr<PipelineState> D3d11PipelineStateManager::CreateGraphicsPipeline(Microsoft::WRL::ComPtr<ID3D11Device>& p_device, const PipelineCreateInfo& p_info, const std::vector<D3D_SHADER_MACRO>& p_defines) {
    auto pipeline_state = std::make_shared<D3d11PipelineState>(p_info.input_layout_desc,
                                                               p_info.rasterizer_desc,
                                                               p_info.depth_stencil_desc);

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> vsblob;
    if (!p_info.vs.empty()) {
        auto res = CompileShader(p_info.vs, "vs_5_0", p_defines.data());
        if (!res) {
            LOG_FATAL("Failed to compile '{}'\n  detail: {}", p_info.vs, res.error());
            return nullptr;
        }

        ComPtr<ID3DBlob> blob;
        blob = *res;
        hr = p_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, pipeline_state->vertexShader.GetAddressOf());
        D3D_FAIL_V_MSG(hr, nullptr, "failed to create vertex buffer");
        vsblob = blob;
    }
    if (!p_info.ps.empty()) {
        auto res = CompileShader(p_info.ps, "ps_5_0", p_defines.data());
        if (!res) {
            LOG_FATAL("Failed to compile '{}'\n  detail: {}", p_info.vs, res.error());
            return nullptr;
        }

        ComPtr<ID3DBlob> blob;
        blob = *res;
        hr = p_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, pipeline_state->pixelShader.GetAddressOf());
        D3D_FAIL_V_MSG(hr, nullptr, "failed to create vertex buffer");
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    elements.reserve(p_info.input_layout_desc->elements.size());
    for (const auto& ele : p_info.input_layout_desc->elements) {
        D3D11_INPUT_ELEMENT_DESC desc;
        desc.SemanticName = ele.semantic_name.c_str();
        desc.SemanticIndex = ele.semantic_index;
        desc.Format = d3d::Convert(ele.format);
        desc.InputSlot = ele.input_slot;
        desc.AlignedByteOffset = ele.aligned_byte_offset;
        desc.InputSlotClass = d3d::Convert(ele.input_slot_class);
        desc.InstanceDataStepRate = ele.instance_data_step_rate;
        elements.emplace_back(desc);
    }
    DEV_ASSERT(elements.size());

    hr = p_device->CreateInputLayout(elements.data(), (UINT)elements.size(), vsblob->GetBufferPointer(), vsblob->GetBufferSize(), pipeline_state->inputLayout.GetAddressOf());
    D3D_FAIL_V_MSG(hr, nullptr, "failed to create input layout");

    if (p_info.rasterizer_desc) {
        ComPtr<ID3D11RasterizerState> state;

        auto it = m_rasterizerStates.find(p_info.rasterizer_desc);
        if (it == m_rasterizerStates.end()) {
            D3D11_RASTERIZER_DESC desc{};
            desc.FillMode = d3d::Convert(p_info.rasterizer_desc->fill_mode);
            desc.CullMode = d3d::Convert(p_info.rasterizer_desc->cull_mode);
            desc.FrontCounterClockwise = p_info.rasterizer_desc->front_counter_clockwise;
            hr = p_device->CreateRasterizerState(&desc, state.GetAddressOf());
            D3D_FAIL_V_MSG(hr, nullptr, "failed to create rasterizer state");
            m_rasterizerStates[p_info.rasterizer_desc] = state;
        } else {
            state = it->second;
        }
        DEV_ASSERT(state);
        pipeline_state->rasterizerState = state;
    }
    if (p_info.depth_stencil_desc) {
        ComPtr<ID3D11DepthStencilState> state;

        auto it = m_depthStencilStates.find(p_info.depth_stencil_desc);
        if (it == m_depthStencilStates.end()) {
            D3D11_DEPTH_STENCIL_DESC desc{};
            desc.DepthEnable = p_info.depth_stencil_desc->depth_enabled;
            desc.DepthFunc = d3d::Convert(p_info.depth_stencil_desc->depth_func);
            desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            desc.StencilEnable = false;
            // desc.StencilEnable = p_info.depth_stencil_desc->stencil_enabled;
            p_device->CreateDepthStencilState(&desc, state.GetAddressOf());
            D3D_FAIL_V_MSG(hr, nullptr, "failed to create depth stencil state");
            m_depthStencilStates[p_info.depth_stencil_desc] = state;
        } else {
            state = it->second.Get();
        }
        DEV_ASSERT(state);
        pipeline_state->depthStencilState = state;
    }

    return pipeline_state;
}

std::shared_ptr<PipelineState> D3d11PipelineStateManager::CreateComputePipeline(Microsoft::WRL::ComPtr<ID3D11Device>& p_device, const PipelineCreateInfo& p_info, const std::vector<D3D_SHADER_MACRO>& p_defines) {
    auto pipeline_state = std::make_shared<D3d11PipelineState>(p_info.input_layout_desc,
                                                               p_info.rasterizer_desc,
                                                               p_info.depth_stencil_desc);

    auto res = CompileShader(p_info.cs, "cs_5_0", p_defines.data());
    if (!res) {
        LOG_ERROR("Failed to compile '{}'\n  detail: {}", p_info.cs, res.error());
        return nullptr;
    }

    ComPtr<ID3DBlob> blob;
    blob = *res;
    HRESULT hr = p_device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, pipeline_state->computeShader.GetAddressOf());
    D3D_FAIL_V_MSG(hr, nullptr, "failed to create vertex buffer");

    return pipeline_state;
}

}  // namespace my
