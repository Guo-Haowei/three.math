#include "d3d11_pipeline_state_manager.h"

#include <d3dcompiler.h>

#include <fstream>

#include "drivers/d3d11/convert.h"
#include "drivers/d3d11/d3d11_helpers.h"

extern ID3D11Device* get_d3d11_device();

namespace my {

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

static auto compile_shader(std::string_view p_path, const char* p_target, const D3D_SHADER_MACRO* p_defines) -> std::expected<ComPtr<ID3DBlob>, std::string> {
    fs::path fullpath = fs::path{ ROOT_FOLDER } / "source" / "shader" / "hlsl" / (std::string(p_path) + ".hlsl");
    std::string fullpath_str = fullpath.string();

    std::wstring path{ fullpath_str.begin(), fullpath_str.end() };
    ComPtr<ID3DBlob> error;
    ComPtr<ID3DBlob> source;

    // @TODO: custom include
    uint32_t flags = D3DCOMPILE_ENABLE_STRICTNESS;
    HRESULT hr = D3DCompileFromFile(
        path.c_str(),
        p_defines,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
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
            return std::unexpected("");
        }
    }

    return source;
}

std::shared_ptr<PipelineState> D3d11PipelineStateManager::create(const PipelineCreateInfo& p_info) {
    ID3D11Device* device = get_d3d11_device();
    DEV_ASSERT(device);
    if (!device) {
        return nullptr;
    }
    auto pipeline_state = std::make_shared<D3d11PipelineState>(p_info.input_layout_desc,
                                                               p_info.rasterizer_desc);

    HRESULT hr = S_OK;

    std::vector<D3D_SHADER_MACRO> macros;
    for (const auto& define : p_info.defines) {
        macros.push_back({ define.name, define.value });
    }
    macros.push_back({ "HLSL_LANG", "1" });
    macros.push_back({ nullptr, nullptr });

    ComPtr<ID3DBlob> vsblob;
    if (!p_info.vs.empty()) {
        auto res = compile_shader(p_info.vs, "vs_5_0", macros.data());
        if (!res) {
            LOG_ERROR("Failed to compile '{}'\n  detail: {}", p_info.vs, res.error());
            return nullptr;
        }

        ComPtr<ID3DBlob> blob;
        blob = *res;
        hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, pipeline_state->vertex_shader.GetAddressOf());
        D3D_FAIL_V_MSG(hr, nullptr, "failed to create vertex buffer");
        vsblob = blob;
    }
    if (!p_info.ps.empty()) {
        auto res = compile_shader(p_info.ps, "ps_5_0", macros.data());
        if (!res) {
            LOG_ERROR("Failed to compile '{}'\n  detail: {}", p_info.vs, res.error());
            return nullptr;
        }

        ComPtr<ID3DBlob> blob;
        blob = *res;
        hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, pipeline_state->pixel_shader.GetAddressOf());
        D3D_FAIL_V_MSG(hr, nullptr, "failed to create vertex buffer");
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    elements.reserve(p_info.input_layout_desc->elements.size());
    for (const auto& ele : p_info.input_layout_desc->elements) {
        D3D11_INPUT_ELEMENT_DESC desc;
        desc.SemanticName = ele.semantic_name.c_str();
        desc.SemanticIndex = ele.semantic_index;
        desc.Format = convert(ele.format);
        desc.InputSlot = ele.input_slot;
        desc.AlignedByteOffset = ele.aligned_byte_offset;
        desc.InputSlotClass = convert(ele.input_slot_class);
        desc.InstanceDataStepRate = ele.instance_data_step_rate;
        elements.emplace_back(desc);
    }
    DEV_ASSERT(elements.size());

    hr = device->CreateInputLayout(elements.data(), (UINT)elements.size(), vsblob->GetBufferPointer(), vsblob->GetBufferSize(), pipeline_state->input_layout.GetAddressOf());
    D3D_FAIL_V_MSG(hr, nullptr, "failed to create input layout");

    {
        auto it = m_rasterizer_states.find((void*)p_info.rasterizer_desc);

        ID3D11RasterizerState* state = nullptr;
        if (it == m_rasterizer_states.end()) {
            D3D11_RASTERIZER_DESC desc{};
            desc.FillMode = convert(p_info.rasterizer_desc->fillMode);
            desc.CullMode = convert(p_info.rasterizer_desc->cullMode);
            desc.FrontCounterClockwise = p_info.rasterizer_desc->frontCounterClockwise;
            hr = device->CreateRasterizerState(&desc, &state);
            D3D_FAIL_V_MSG(hr, nullptr, "failed to create rasterizer state");
            m_rasterizer_states[(void*)p_info.rasterizer_desc] = state;
        } else {
            state = it->second.Get();
        }
        DEV_ASSERT(state);
        pipeline_state->rasterizer = state;
    }

    return pipeline_state;
}

}  // namespace my
