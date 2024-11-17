#include "d3d11_graphics_manager.h"

#include <imgui/backends/imgui_impl_dx11.h>

#include "drivers/d3d11/d3d11_helpers.h"
#include "drivers/d3d11/d3d11_pipeline_state_manager.h"
#include "drivers/d3d11/d3d11_resources.h"
#include "drivers/d3d_common/d3d_common.h"
#include "drivers/windows/win32_display_manager.h"
#include "rendering/gpu_resource.h"
#include "rendering/graphics_dvars.h"
#include "rendering/render_graph/render_graph_defines.h"

#define INCLUDE_AS_D3D11
#include "drivers/d3d_common/d3d_convert.h"

namespace my {

using Microsoft::WRL::ComPtr;

D3d11GraphicsManager::D3d11GraphicsManager() : GraphicsManager("D3d11GraphicsManager", Backend::D3D11, 1) {
    m_pipelineStateManager = std::make_shared<D3d11PipelineStateManager>();
}

bool D3d11GraphicsManager::InitializeImpl() {
    bool ok = true;
    ok = ok && CreateDevice();
    ok = ok && CreateSwapChain();
    ok = ok && CreateRenderTarget();
    ok = ok && InitSamplers();
    ok = ok && ImGui_ImplDX11_Init(m_device.Get(), m_deviceContext.Get());

    ImGui_ImplDX11_NewFrame();

    SelectRenderGraph();

    // @TODO: refactor this
    m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_meshes.set_description("GPU-Mesh-Allocator");
    return ok;
}

void D3d11GraphicsManager::Finalize() {
    ImGui_ImplDX11_Shutdown();
}

void D3d11GraphicsManager::Render() {
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_deviceContext->OMSetRenderTargets(1, m_windowRtv.GetAddressOf(), nullptr);
    m_deviceContext->ClearRenderTargetView(m_windowRtv.Get(), clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void D3d11GraphicsManager::Present() {
    ImGuiIO& io = ImGui::GetIO();
    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    m_swapChain->Present(1, 0);  // Present with vsync
}

void D3d11GraphicsManager::SetStencilRef(uint32_t p_ref) {
    unused(p_ref);
}

void D3d11GraphicsManager::Dispatch(uint32_t p_num_groups_x, uint32_t p_num_groups_y, uint32_t p_num_groups_z) {
    m_deviceContext->Dispatch(p_num_groups_x, p_num_groups_y, p_num_groups_z);
}

void D3d11GraphicsManager::SetUnorderedAccessView(uint32_t p_slot, GpuTexture* p_texture) {
    if (!p_texture) {
        ID3D11UnorderedAccessView* uav = nullptr;
        m_deviceContext->CSSetUnorderedAccessViews(p_slot, 1, &uav, nullptr);
        return;
    }

    auto texture = dynamic_cast<D3d11GpuTexture*>(p_texture);

    m_deviceContext->CSSetUnorderedAccessViews(p_slot, 1, texture->uav.GetAddressOf(), nullptr);
}

void D3d11GraphicsManager::OnWindowResize(int p_width, int p_height) {
    if (m_device) {
        m_windowRtv.Reset();
        m_swapChain->ResizeBuffers(0, p_width, p_height, DXGI_FORMAT_UNKNOWN, 0);
        CreateRenderTarget();
    }
}

bool D3d11GraphicsManager::CreateDevice() {
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_1;
    UINT create_device_flags = m_enableValidationLayer ? D3D11_CREATE_DEVICE_DEBUG : 0;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        create_device_flags,
        &feature_level,
        1,
        D3D11_SDK_VERSION,
        m_device.GetAddressOf(),
        nullptr,
        m_deviceContext.GetAddressOf());

    D3D_FAIL_V_MSG(hr, false, "Failed to create d3d11 device");

    D3D_FAIL_V_MSG(m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)m_dxgiDevice.GetAddressOf()),
                   false,
                   "Failed to query IDXGIDevice");

    D3D_FAIL_V_MSG(m_dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)m_dxgiAdapter.GetAddressOf()),
                   false,
                   "Failed to query IDXGIAdapter");

    D3D_FAIL_V_MSG(m_dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)m_dxgiFactory.GetAddressOf()),
                   false,
                   "Failed to query IDXGIFactory");

    return true;
}

bool D3d11GraphicsManager::CreateSwapChain() {
    auto display_manager = dynamic_cast<Win32DisplayManager*>(DisplayManager::GetSingletonPtr());
    DEV_ASSERT(display_manager);

    DXGI_MODE_DESC buffer_desc{};
    buffer_desc.Width = 0;
    buffer_desc.Height = 0;
    buffer_desc.RefreshRate = { 60, 1 };
    buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
    swap_chain_desc.BufferDesc = buffer_desc;
    swap_chain_desc.SampleDesc = { 1, 0 };
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow = display_manager->GetHwnd();
    swap_chain_desc.Windowed = true;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FAIL_V_MSG(m_dxgiFactory->CreateSwapChain(m_device.Get(), &swap_chain_desc, m_swapChain.GetAddressOf()),
                   false,
                   "Failed to create swap chain");

    return true;
}

bool D3d11GraphicsManager::CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> back_buffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
    m_device->CreateRenderTargetView(back_buffer.Get(), nullptr, m_windowRtv.GetAddressOf());
    return true;
}

bool D3d11GraphicsManager::CreateSampler(uint32_t p_slot, D3D11_SAMPLER_DESC p_desc) {
    ComPtr<ID3D11SamplerState> sampler_state;
    D3D_FAIL_V(m_device->CreateSamplerState(&p_desc, sampler_state.GetAddressOf()), false);

    m_deviceContext->CSSetSamplers(p_slot, 1, sampler_state.GetAddressOf());
    m_deviceContext->PSSetSamplers(p_slot, 1, sampler_state.GetAddressOf());
    m_samplers.emplace_back(sampler_state);
    return true;
}

bool D3d11GraphicsManager::InitSamplers() {
    auto FillSamplerDesc = [](const SamplerDesc& p_desc) {
        D3D11_SAMPLER_DESC sampler_desc;

        sampler_desc.Filter = d3d::Convert(p_desc.minFilter, p_desc.magFilter);
        sampler_desc.AddressU = d3d::Convert(p_desc.addressU);
        sampler_desc.AddressV = d3d::Convert(p_desc.addressV);
        sampler_desc.AddressW = d3d::Convert(p_desc.addressW);
        sampler_desc.MipLODBias = p_desc.mipLodBias;
        sampler_desc.MaxAnisotropy = p_desc.maxAnisotropy;
        sampler_desc.ComparisonFunc = d3d::Convert(p_desc.comparisonFunc);
        sampler_desc.BorderColor[0] = p_desc.border[0];
        sampler_desc.BorderColor[1] = p_desc.border[1];
        sampler_desc.BorderColor[2] = p_desc.border[2];
        sampler_desc.BorderColor[3] = p_desc.border[3];
        sampler_desc.MinLOD = p_desc.minLod;
        sampler_desc.MaxLOD = p_desc.maxLod;
        return sampler_desc;
    };

    bool ok = true;
#define SAMPLER_STATE(REG, NAME, DESC) ok = ok && CreateSampler(REG, FillSamplerDesc(DESC))
#include "sampler.hlsl.h"
#undef SAMPLER_STATE
    return ok;
}

std::shared_ptr<GpuConstantBuffer> D3d11GraphicsManager::CreateConstantBuffer(const GpuBufferDesc& p_desc) {
    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = p_desc.elementCount * p_desc.elementSize;
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;

    ComPtr<ID3D11Buffer> d3d_buffer;
    D3D_FAIL_V(m_device->CreateBuffer(&buffer_desc, nullptr, d3d_buffer.GetAddressOf()), nullptr);

    auto uniform_buffer = std::make_shared<D3d11UniformBuffer>(p_desc);
    uniform_buffer->internalBuffer = d3d_buffer;

    m_deviceContext->VSSetConstantBuffers(p_desc.slot, 1, uniform_buffer->internalBuffer.GetAddressOf());
    m_deviceContext->PSSetConstantBuffers(p_desc.slot, 1, uniform_buffer->internalBuffer.GetAddressOf());
    m_deviceContext->CSSetConstantBuffers(p_desc.slot, 1, uniform_buffer->internalBuffer.GetAddressOf());
    return uniform_buffer;
}

std::shared_ptr<GpuStructuredBuffer> D3d11GraphicsManager::CreateStructuredBuffer(const GpuBufferDesc& p_desc) {
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11ShaderResourceView> srv;

    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.ByteWidth = p_desc.elementCount * p_desc.elementSize;
    buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.StructureByteStride = p_desc.elementSize;

    D3D_FAIL_V_MSG(m_device->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf()),
                   nullptr,
                   "Failed to create buffer (StructuredBuffer)");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = p_desc.elementCount;
    D3D_FAIL_V_MSG(m_device->CreateUnorderedAccessView(buffer.Get(), &uav_desc, uav.GetAddressOf()),
                   nullptr,
                   "Failed to create UAV (StructuredBuffer)");

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.FirstElement = 0;
    srv_desc.Buffer.NumElements = p_desc.elementCount;
    D3D_FAIL_V_MSG(m_device->CreateShaderResourceView(buffer.Get(), &srv_desc, srv.GetAddressOf()),
                   nullptr,
                   "Failed to create SRV (StructuredBuffer)");

    auto structured_buffer = std::make_shared<D3d11StructuredBuffer>(p_desc);
    structured_buffer->buffer = buffer;
    structured_buffer->uav = uav;
    structured_buffer->srv = srv;

    return structured_buffer;
}

void D3d11GraphicsManager::UpdateConstantBuffer(const GpuConstantBuffer* p_buffer, const void* p_data, size_t p_size) {
    auto buffer = reinterpret_cast<const D3d11UniformBuffer*>(p_buffer);
    DEV_ASSERT(p_size <= buffer->capacity);
    buffer->data = (const char*)p_data;
}

void D3d11GraphicsManager::BindConstantBufferRange(const GpuConstantBuffer* p_buffer, uint32_t p_size, uint32_t p_offset) {
    auto buffer = reinterpret_cast<const D3d11UniformBuffer*>(p_buffer);
    DEV_ASSERT(p_size + p_offset <= buffer->capacity);
    D3D11_MAPPED_SUBRESOURCE mapped;
    ZeroMemory(&mapped, sizeof(D3D11_MAPPED_SUBRESOURCE));
    m_deviceContext->Map(buffer->internalBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, buffer->data + p_offset, p_size);
    m_deviceContext->Unmap(buffer->internalBuffer.Get(), 0);
}

void D3d11GraphicsManager::BindTexture(Dimension p_dimension, uint64_t p_handle, int p_slot) {
    unused(p_dimension);

    if (p_handle) {
        ID3D11ShaderResourceView* srv = (ID3D11ShaderResourceView*)(p_handle);
        m_deviceContext->PSSetShaderResources(p_slot, 1, &srv);
        m_deviceContext->CSSetShaderResources(p_slot, 1, &srv);
    }
}

void D3d11GraphicsManager::UnbindTexture(Dimension p_dimension, int p_slot) {
    unused(p_dimension);

    ID3D11ShaderResourceView* srv = nullptr;
    m_deviceContext->PSSetShaderResources(p_slot, 1, &srv);
    m_deviceContext->CSSetShaderResources(p_slot, 1, &srv);
}

void D3d11GraphicsManager::BindStructuredBuffer(int p_slot, const GpuStructuredBuffer* p_buffer) {
    auto structured_buffer = reinterpret_cast<const D3d11StructuredBuffer*>(p_buffer);
    m_deviceContext->CSSetUnorderedAccessViews(p_slot, 1, structured_buffer->uav.GetAddressOf(), nullptr);
}

void D3d11GraphicsManager::UnbindStructuredBuffer(int p_slot) {
    ID3D11UnorderedAccessView* uav = nullptr;
    m_deviceContext->CSSetUnorderedAccessViews(p_slot, 1, &uav, nullptr);
}

void D3d11GraphicsManager::BindStructuredBufferSRV(int p_slot, const GpuStructuredBuffer* p_buffer) {
    auto structured_buffer = reinterpret_cast<const D3d11StructuredBuffer*>(p_buffer);

    if (structured_buffer->srv != nullptr) {
        m_deviceContext->VSSetShaderResources(p_slot, 1, structured_buffer->srv.GetAddressOf());
    }
}

void D3d11GraphicsManager::UnbindStructuredBufferSRV(int p_slot) {
    ID3D11ShaderResourceView* srv = nullptr;
    m_deviceContext->VSSetShaderResources(p_slot, 1, &srv);
}

std::shared_ptr<GpuTexture> D3d11GraphicsManager::CreateGpuTextureImpl(const GpuTextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc) {
    unused(p_sampler_desc);

    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;

    PixelFormat format = p_texture_desc.format;
    DXGI_FORMAT texture_format = d3d::Convert(format);
    DXGI_FORMAT srv_format = d3d::Convert(format);
    // @TODO: fix this
    bool gen_mip_map = p_texture_desc.bindFlags & BIND_SHADER_RESOURCE;
    if (p_texture_desc.dimension == Dimension::TEXTURE_CUBE) {
        gen_mip_map = false;
    }

    // @TODO: refactor this part
    if (format == PixelFormat::D32_FLOAT) {
        texture_format = DXGI_FORMAT_R32_TYPELESS;
        srv_format = DXGI_FORMAT_R32_FLOAT;
        gen_mip_map = false;
    }
    if (format == PixelFormat::D24_UNORM_S8_UINT) {
        texture_format = DXGI_FORMAT_R24G8_TYPELESS;
    }
    if (format == PixelFormat::R24G8_TYPELESS) {
        srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        gen_mip_map = false;
    }

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = p_texture_desc.width;
    texture_desc.Height = p_texture_desc.height;
    texture_desc.MipLevels = gen_mip_map ? 0 : p_texture_desc.mipLevels;
    // texture_desc.MipLevels = 0;
    texture_desc.ArraySize = p_texture_desc.arraySize;
    texture_desc.Format = texture_format;
    texture_desc.SampleDesc = { 1, 0 };
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = d3d::ConvertBindFlags(p_texture_desc.bindFlags);
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = d3d::ConvertResourceMiscFlags(p_texture_desc.miscFlags);
    ComPtr<ID3D11Texture2D> texture;
    D3D_FAIL_V(m_device->CreateTexture2D(&texture_desc, nullptr, texture.GetAddressOf()), nullptr);

    const char* debug_name = RenderTargetResourceNameToString(p_texture_desc.name);

    texture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen(debug_name)), debug_name);

    if (p_texture_desc.initialData) {
        uint32_t row_pitch = p_texture_desc.width * channel_count(format) * channel_size(format);
        m_deviceContext->UpdateSubresource(texture.Get(), 0, nullptr, p_texture_desc.initialData, row_pitch, 0);
    }

    auto gpu_texture = std::make_shared<D3d11GpuTexture>(p_texture_desc);
    if (p_texture_desc.bindFlags & BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = srv_format;
        srv_desc.ViewDimension = ConvertDimension(p_texture_desc.dimension);
        srv_desc.Texture2D.MipLevels = p_texture_desc.mipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;

        if (gen_mip_map) {
            srv_desc.Texture2D.MipLevels = (UINT)-1;
        }

        D3D_FAIL_V_MSG(m_device->CreateShaderResourceView(texture.Get(), &srv_desc, srv.GetAddressOf()),
                       nullptr,
                       "Failed to create shader resource view");

        if (p_texture_desc.miscFlags & RESOURCE_MISC_GENERATE_MIPS) {
            m_deviceContext->GenerateMips(srv.Get());
        }

        gpu_texture->srv = srv;
    }

    // @TODO: only generate uav when necessary
    if (texture_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        // Create UAV
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = texture_format;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;

        D3D_FAIL_V_MSG(m_device->CreateUnorderedAccessView(texture.Get(), &uavDesc, uav.GetAddressOf()),
                       nullptr,
                       "Failed to create unordered access view");

        gpu_texture->uav = uav;
    }

    SetDebugName(texture.Get(), RenderTargetResourceNameToString(p_texture_desc.name));
    gpu_texture->texture = texture;

    return gpu_texture;
}

std::shared_ptr<DrawPass> D3d11GraphicsManager::CreateDrawPass(const DrawPassDesc& p_subpass_desc) {
    auto draw_pass = std::make_shared<D3d11DrawPass>();
    draw_pass->execFunc = p_subpass_desc.execFunc;
    draw_pass->colorAttachments = p_subpass_desc.colorAttachments;
    draw_pass->depthAttachment = p_subpass_desc.depthAttachment;

    for (const auto& color_attachment : p_subpass_desc.colorAttachments) {
        auto texture = reinterpret_cast<const D3d11GpuTexture*>(color_attachment.get());
        switch (color_attachment->desc.type) {
            case AttachmentType::COLOR_2D: {
                ComPtr<ID3D11RenderTargetView> rtv;
                D3D_FAIL_V(m_device->CreateRenderTargetView(texture->texture.Get(), nullptr, rtv.GetAddressOf()), nullptr);
                draw_pass->rtvs.emplace_back(rtv);
            } break;
            default:
                CRASH_NOW();
                break;
        }
    }

    if (auto& depth_attachment = draw_pass->depthAttachment; depth_attachment) {
        auto texture = reinterpret_cast<const D3d11GpuTexture*>(depth_attachment.get());
        switch (depth_attachment->desc.type) {
            case AttachmentType::DEPTH_2D: {
                ComPtr<ID3D11DepthStencilView> dsv;
                D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
                dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dsv_desc.Texture2D.MipSlice = 0;

                D3D_FAIL_V(m_device->CreateDepthStencilView(texture->texture.Get(), &dsv_desc, dsv.GetAddressOf()), nullptr);
                draw_pass->dsvs.push_back(dsv);
            } break;
            case AttachmentType::DEPTH_STENCIL_2D: {
                ComPtr<ID3D11DepthStencilView> dsv;
                D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dsv_desc.Texture2D.MipSlice = 0;

                D3D_FAIL_V(m_device->CreateDepthStencilView(texture->texture.Get(), &dsv_desc, dsv.GetAddressOf()), nullptr);
                draw_pass->dsvs.push_back(dsv);
            } break;
            case AttachmentType::SHADOW_2D: {
                ComPtr<ID3D11DepthStencilView> dsv;
                D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
                dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dsv_desc.Texture2D.MipSlice = 0;

                D3D_FAIL_V(m_device->CreateDepthStencilView(texture->texture.Get(), &dsv_desc, dsv.GetAddressOf()), nullptr);
                draw_pass->dsvs.push_back(dsv);
            } break;
            case AttachmentType::SHADOW_CUBE_MAP: {
                for (int face = 0; face < 6; ++face) {
                    ComPtr<ID3D11DepthStencilView> dsv;
                    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
                    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                    dsv_desc.Texture2DArray.MipSlice = 0;
                    dsv_desc.Texture2DArray.ArraySize = 1;
                    dsv_desc.Texture2DArray.FirstArraySlice = face;

                    D3D_FAIL_V(m_device->CreateDepthStencilView(texture->texture.Get(), &dsv_desc, dsv.GetAddressOf()), nullptr);
                    draw_pass->dsvs.push_back(dsv);
                }
            } break;
            default:
                CRASH_NOW();
                break;
        }
    }

    return draw_pass;
}

void D3d11GraphicsManager::SetRenderTarget(const DrawPass* p_draw_pass, int p_index, int p_mip_level) {
    unused(p_mip_level);
    DEV_ASSERT(p_draw_pass);

    auto draw_pass = reinterpret_cast<const D3d11DrawPass*>(p_draw_pass);
    if (const auto depth_attachment = draw_pass->depthAttachment; depth_attachment) {
        if (depth_attachment->desc.type == AttachmentType::SHADOW_CUBE_MAP) {
            ID3D11RenderTargetView* rtv = nullptr;
            m_deviceContext->OMSetRenderTargets(1, &rtv, draw_pass->dsvs[p_index].Get());
            return;
        }
    }

    // @TODO: fixed_vector
    std::vector<ID3D11RenderTargetView*> rtvs;
    for (auto& rtv : draw_pass->rtvs) {
        rtvs.emplace_back(rtv.Get());
    }

    ID3D11DepthStencilView* dsv = draw_pass->dsvs.size() ? draw_pass->dsvs[0].Get() : nullptr;
    m_deviceContext->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), dsv);
}

void D3d11GraphicsManager::UnsetRenderTarget() {
    ID3D11RenderTargetView* rtvs[] = { nullptr, nullptr, nullptr, nullptr };
    m_deviceContext->OMSetRenderTargets(array_length(rtvs), rtvs, nullptr);
}

void D3d11GraphicsManager::Clear(const DrawPass* p_draw_pass, ClearFlags p_flags, const float* p_clear_color, int p_index) {
    auto draw_pass = reinterpret_cast<const D3d11DrawPass*>(p_draw_pass);

    if (p_flags & CLEAR_COLOR_BIT) {
        for (auto& rtv : draw_pass->rtvs) {
            m_deviceContext->ClearRenderTargetView(rtv.Get(), p_clear_color);
        }
    }

    uint32_t clear_flags = 0;
    if (p_flags & CLEAR_DEPTH_BIT) {
        clear_flags |= D3D11_CLEAR_DEPTH;
    }
    if (p_flags & CLEAR_STENCIL_BIT) {
        clear_flags |= D3D11_CLEAR_STENCIL;
    }
    if (clear_flags) {
        // @TODO: better way?
        DEV_ASSERT_INDEX(p_index, draw_pass->dsvs.size());
        m_deviceContext->ClearDepthStencilView(draw_pass->dsvs[p_index].Get(), clear_flags, 1.0f, 0);
    }
}

void D3d11GraphicsManager::SetViewport(const Viewport& p_viewport) {
    D3D11_VIEWPORT vp{};
    // @TODO: gl and d3d use different viewport
    vp.TopLeftX = static_cast<float>(p_viewport.topLeftX);
    vp.TopLeftY = static_cast<float>(p_viewport.topLeftY);
    vp.Width = static_cast<float>(p_viewport.width);
    vp.Height = static_cast<float>(p_viewport.height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_deviceContext->RSSetViewports(1, &vp);
}

const MeshBuffers* D3d11GraphicsManager::CreateMesh(const MeshComponent& p_mesh) {
    auto createMesh_data = [](ID3D11Device* p_device, const MeshComponent& mesh, D3d11MeshBuffers& out_mesh) {
        auto create_vertex_buffer = [&](size_t p_size_in_byte, const void* p_data) -> ID3D11Buffer* {
            ID3D11Buffer* buffer = nullptr;
            // vertex buffer
            D3D11_BUFFER_DESC bufferDesc{};
            bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            bufferDesc.ByteWidth = (UINT)p_size_in_byte;
            bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;

            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = p_data;
            D3D_FAIL_V_MSG(p_device->CreateBuffer(&bufferDesc, &data, &buffer),
                           nullptr,
                           "Failed to Create vertex buffer");
            return buffer;
        };

        out_mesh.vertex_buffer[0] = create_vertex_buffer(sizeof(vec3) * mesh.positions.size(), mesh.positions.data());

        if (!mesh.normals.empty()) {
            out_mesh.vertex_buffer[1] = create_vertex_buffer(sizeof(vec3) * mesh.normals.size(), mesh.normals.data());
        }

        if (!mesh.texcoords_0.empty()) {
            out_mesh.vertex_buffer[2] = create_vertex_buffer(sizeof(vec2) * mesh.texcoords_0.size(), mesh.texcoords_0.data());
        }

        if (!mesh.joints_0.empty()) {
            out_mesh.vertex_buffer[4] = create_vertex_buffer(sizeof(ivec4) * mesh.joints_0.size(), mesh.joints_0.data());
        }
        if (!mesh.weights_0.empty()) {
            out_mesh.vertex_buffer[5] = create_vertex_buffer(sizeof(vec4) * mesh.weights_0.size(), mesh.weights_0.data());
        }
        {
            // index buffer
            out_mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
            D3D11_BUFFER_DESC bufferDesc{};
            bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            bufferDesc.ByteWidth = static_cast<uint32_t>(sizeof(uint32_t) * out_mesh.indexCount);
            bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;

            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = mesh.indices.data();
            D3D_FAIL_MSG(p_device->CreateBuffer(&bufferDesc, &data, out_mesh.index_buffer.GetAddressOf()),
                         "Failed to create index buffer");
        }
    };

    RID rid = m_meshes.make_rid();
    D3d11MeshBuffers* mesh_buffers = m_meshes.get_or_null(rid);
    p_mesh.gpuResource = mesh_buffers;
    createMesh_data(m_device.Get(), p_mesh, *mesh_buffers);
    return mesh_buffers;
}

void D3d11GraphicsManager::SetMesh(const MeshBuffers* p_mesh) {
    auto mesh = reinterpret_cast<const D3d11MeshBuffers*>(p_mesh);

    ID3D11Buffer* buffers[6] = {
        mesh->vertex_buffer[0].Get(),
        mesh->vertex_buffer[1].Get(),
        mesh->vertex_buffer[2].Get(),
        mesh->vertex_buffer[3].Get(),
        mesh->vertex_buffer[4].Get(),
        mesh->vertex_buffer[5].Get(),
    };

    UINT stride[6] = {
        sizeof(vec3),
        sizeof(vec3),
        sizeof(vec2),
        sizeof(vec3),
        sizeof(ivec4),
        sizeof(vec4),
    };

    UINT offset[6] = { 0, 0, 0, 0, 0, 0 };

    // @TODO: fix
    m_deviceContext->IASetVertexBuffers(0, 6, buffers, stride, offset);
    m_deviceContext->IASetIndexBuffer(mesh->index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
}

void D3d11GraphicsManager::DrawElements(uint32_t p_count, uint32_t p_offset) {
    m_deviceContext->DrawIndexed(p_count, p_offset, 0);
}

void D3d11GraphicsManager::DrawElementsInstanced(uint32_t p_instance_count, uint32_t p_count, uint32_t p_offset) {
    m_deviceContext->DrawIndexedInstanced(p_count, p_instance_count, p_offset, 0, 0);
}

void D3d11GraphicsManager::OnSceneChange(const Scene& p_scene) {
    for (auto [entity, mesh] : p_scene.m_MeshComponents) {
        if (mesh.gpuResource != nullptr) {
            const NameComponent& name = *p_scene.GetComponent<NameComponent>(entity);
            LOG_WARN("[begin_scene] mesh '{}' () already has gpu resource", name.GetName());
            continue;
        }

        CreateMesh(mesh);
    }
}

void D3d11GraphicsManager::SetPipelineStateImpl(PipelineStateName p_name) {
    auto pipeline = reinterpret_cast<D3d11PipelineState*>(m_pipelineStateManager->Find(p_name));
    DEV_ASSERT(pipeline);
    if (pipeline->computeShader) {
        m_deviceContext->CSSetShader(pipeline->computeShader.Get(), nullptr, 0);
    } else {
        if (pipeline->vertexShader) {
            m_deviceContext->VSSetShader(pipeline->vertexShader.Get(), 0, 0);
            m_deviceContext->IASetInputLayout(pipeline->inputLayout.Get());
        }
        if (pipeline->pixelShader) {
            m_deviceContext->PSSetShader(pipeline->pixelShader.Get(), 0, 0);
        }

        if (pipeline->rasterizerState.Get() != m_stateCache.rasterizer) {
            m_deviceContext->RSSetState(pipeline->rasterizerState.Get());
            m_stateCache.rasterizer = pipeline->rasterizerState.Get();
        }

        if (pipeline->depthStencilState.Get() != m_stateCache.depth_stencil) {
            m_deviceContext->OMSetDepthStencilState(pipeline->depthStencilState.Get(), 0);
            m_stateCache.depth_stencil = pipeline->depthStencilState.Get();
        }
    }
}

}  // namespace my

#undef INCLUDE_AS_D3D11
