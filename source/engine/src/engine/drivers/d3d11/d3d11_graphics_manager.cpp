#include "d3d11_graphics_manager.h"

#include <dxgi.h>
#include <imgui/backends/imgui_impl_dx11.h>

#include "drivers/d3d11/d3d11_helpers.h"
#include "drivers/d3d11/d3d11_resources.h"
#include "drivers/windows/win32_display_manager.h"
#include "rendering/rendering_dvars.h"
#include "rendering/texture.h"

namespace my {

using Microsoft::WRL::ComPtr;

// @TODO: render target
ComPtr<ID3D11Texture2D> m_render_target_texture;
ComPtr<ID3D11RenderTargetView> m_render_target_view;
ComPtr<ID3D11ShaderResourceView> m_srv;

bool D3d11GraphicsManager::initialize() {
    bool ok = true;
    ok = ok && create_device();
    ok = ok && create_swap_chain();
    ok = ok && create_render_target();
    ok = ok && ImGui_ImplDX11_Init(m_device.Get(), m_ctx.Get());

    ImGui_ImplDX11_NewFrame();

    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width = 1024;   // Example width
    texture_desc.Height = 1024;  // Example height
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // RGBA format
    texture_desc.SampleDesc = { 1, 0 };
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;

    HRESULT hr = S_OK;
    hr = m_device->CreateTexture2D(&texture_desc, nullptr, m_render_target_texture.GetAddressOf());
    CRASH_COND(FAILED(hr));

    // D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};

    hr = m_device->CreateRenderTargetView(m_render_target_texture.Get(), nullptr, m_render_target_view.GetAddressOf());
    CRASH_COND(FAILED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texture_desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    hr = m_device->CreateShaderResourceView(m_render_target_texture.Get(), &srvDesc, &m_srv);
    CRASH_COND(FAILED(hr));

    select_render_graph();

    return ok;
}

void D3d11GraphicsManager::finalize() {
    ImGui_ImplDX11_Shutdown();
}

uint64_t D3d11GraphicsManager::get_final_image() const {
    return (uint64_t)m_srv.Get();
}

void D3d11GraphicsManager::render() {
    const float clear_color[4] = { 0.3f, 0.4f, 0.3f, 1.0f };
    m_ctx->OMSetRenderTargets(1, m_render_target_view.GetAddressOf(), nullptr);
    m_ctx->ClearRenderTargetView(m_render_target_view.Get(), clear_color);

    // ID3D11RenderTargetView* rtv = nullptr;
    // m_ctx->OMSetRenderTargets(1, &rtv, nullptr);

    m_ctx->OMSetRenderTargets(1, m_window_rtv.GetAddressOf(), nullptr);
    m_ctx->ClearRenderTargetView(m_window_rtv.Get(), clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    m_swap_chain->Present(1, 0);  // Present with vsync
}

void D3d11GraphicsManager::on_window_resize(int p_width, int p_height) {
    if (m_device) {
        m_window_rtv.Reset();
        m_swap_chain->ResizeBuffers(0, p_width, p_height, DXGI_FORMAT_UNKNOWN, 0);
        create_render_target();
    }
}

bool D3d11GraphicsManager::create_device() {
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    UINT create_device_flags = 0;
    if (DVAR_GET_BOOL(r_gpu_validation)) {
        create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

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
        m_ctx.GetAddressOf());

    D3D_FAIL_V_MSG(hr, false, "Failed to create d3d11 device");

    D3D_FAIL_V_MSG(m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)m_dxgi_device.GetAddressOf()),
                   false,
                   "Failed to query IDXGIDevice");

    D3D_FAIL_V_MSG(m_dxgi_device->GetParent(__uuidof(IDXGIAdapter), (void**)m_dxgi_adapter.GetAddressOf()),
                   false,
                   "Failed to query IDXGIAdapter");

    D3D_FAIL_V_MSG(m_dxgi_adapter->GetParent(__uuidof(IDXGIFactory), (void**)m_dxgi_factory.GetAddressOf()),
                   false,
                   "Failed to query IDXGIFactory");

    return true;
}

bool D3d11GraphicsManager::create_swap_chain() {
    auto display_manager = dynamic_cast<Win32DisplayManager*>(DisplayManager::singleton_ptr());
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
    swap_chain_desc.OutputWindow = display_manager->get_hwnd();
    swap_chain_desc.Windowed = true;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FAIL_V_MSG(m_dxgi_factory->CreateSwapChain(m_device.Get(), &swap_chain_desc, m_swap_chain.GetAddressOf()),
                   false,
                   "Failed to create swap chain");

    return true;
}

bool D3d11GraphicsManager::create_render_target() {
    ComPtr<ID3D11Texture2D> back_buffer;
    m_swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
    m_device->CreateRenderTargetView(back_buffer.Get(), nullptr, m_window_rtv.GetAddressOf());
    return true;
}

inline uint32_t convert_bind_flags(uint32_t p_bind_flags) {
    // only support a few flags for now
    DEV_ASSERT((p_bind_flags & (~(BIND_SHADER_RESOURCE | BIND_RENDER_TARGET))) == 0);

    uint32_t flags = 0;
    if (p_bind_flags & BIND_SHADER_RESOURCE) {
        flags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (p_bind_flags & BIND_RENDER_TARGET) {
        flags |= D3D11_BIND_RENDER_TARGET;
    }

    return flags;
}

inline uint32_t convert_misc_flags(uint32_t p_misc_flags) {
    // only support a few flags for now
    DEV_ASSERT((p_misc_flags & (~RESOURCE_MISC_GENERATE_MIPS)) == 0);

    uint32_t flags = 0;
    if (p_misc_flags & RESOURCE_MISC_GENERATE_MIPS) {
        flags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    return flags;
}

std::shared_ptr<Texture> D3d11GraphicsManager::create_texture(const TextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc) {
    unused(p_sampler_desc);

    ComPtr<ID3D11ShaderResourceView> srv;

    PixelFormat format = p_texture_desc.format;

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = p_texture_desc.width;
    texture_desc.Height = p_texture_desc.height;
    texture_desc.MipLevels = p_texture_desc.mip_levels;
    texture_desc.ArraySize = p_texture_desc.array_size;
    texture_desc.Format = d3d11::convert_format(format);
    texture_desc.SampleDesc = { 1, 0 };
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = convert_bind_flags(p_texture_desc.bind_flags);
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = convert_misc_flags(p_texture_desc.misc_flags);

    D3D11_SUBRESOURCE_DATA texture_data{};
    ComPtr<ID3D11Texture2D> texture;
    texture_data.pSysMem = p_texture_desc.initial_data;
    texture_data.SysMemPitch = p_texture_desc.width * channel_count(format) * channel_size(format);
    texture_data.SysMemSlicePitch = p_texture_desc.height * texture_data.SysMemPitch;

    D3D11_SUBRESOURCE_DATA* texture_data_ptr = p_texture_desc.initial_data ? &texture_data : nullptr;
    D3D_FAIL_V_MSG(m_device->CreateTexture2D(&texture_desc, texture_data_ptr, texture.GetAddressOf()),
                   nullptr,
                   "Failed to create texture");

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = texture_desc.Format;
    srv_desc.ViewDimension = d3d11::convert_dimension(p_texture_desc.dimension);
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;

    D3D_FAIL_V_MSG(m_device->CreateShaderResourceView(texture.Get(), &srv_desc, srv.GetAddressOf()),
                   nullptr,
                   "Failed to create shader resource view");

    auto gpu_texture = std::make_shared<D3d11Texture>(p_texture_desc);
    gpu_texture->srv = srv;
    return gpu_texture;
}

std::shared_ptr<Subpass> D3d11GraphicsManager::create_subpass(const SubpassDesc& p_subpass_desc) {
    unused(p_subpass_desc);
    return nullptr;
}

}  // namespace my
