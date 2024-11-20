#include "d3d12_graphics_manager.h"

#include <imgui/backends/imgui_impl_dx12.h>

#include "core/string/string_utils.h"
#include "drivers/d3d12/d3d12_pipeline_state_manager.h"
#include "drivers/d3d_common/d3d_common.h"
#include "drivers/windows/win32_display_manager.h"
#include "rendering/graphics_private.h"

#define INCLUDE_AS_D3D12
#include "drivers/d3d_common/d3d_convert.h"

namespace my {

// @TODO: refactor
struct D3d12GpuTexture : public GpuTexture {
    using GpuTexture::GpuTexture;

    uint64_t GetResidentHandle() const final {
        uint64_t handle = index;
        switch (desc.dimension) {
            case Dimension::TEXTURE_2D:
                return handle;
            case Dimension::TEXTURE_CUBE_ARRAY:
                // @TODO: refactor
                return handle - MAX_TEXTURE_2D_COUNT;
            default:
                CRASH_NOW();
                return 0;
        }
    }
    uint64_t GetHandle() const final { return gpuHandle; }

    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    int index{ -1 };
    size_t cpuHandle{ 0 };
    uint64_t gpuHandle{ 0 };
};

struct D3d12DrawPass : public DrawPass {
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsvs;
};

struct D3d12ConstantBuffer : public GpuConstantBuffer {
    using GpuConstantBuffer::GpuConstantBuffer;

    ~D3d12ConstantBuffer() {
        if (buffer) {
            buffer->Unmap(0, nullptr);
        }
        mappedData = nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer{};
    uint8_t* mappedData{ nullptr };
};

struct D3d12FrameContext : FrameContext {
    ~D3d12FrameContext() {
        SafeRelease(m_commandAllocator);
    }

    void Wait(HANDLE p_fence_event, ID3D12Fence1* p_fence) {
        if (p_fence->GetCompletedValue() < m_fenceValue) {
            D3D_CALL(p_fence->SetEventOnCompletion(m_fenceValue, p_fence_event));
            WaitForSingleObject(p_fence_event, INFINITE);
        }
    }

    ID3D12CommandAllocator* m_commandAllocator = nullptr;
    uint64_t m_fenceValue = 0;
};

using Microsoft::WRL::ComPtr;

D3d12GraphicsManager::D3d12GraphicsManager() : GraphicsManager("D3d12GraphicsManager", Backend::D3D12, NUM_FRAMES_IN_FLIGHT) {
    m_pipelineStateManager = std::make_shared<D3d12PipelineStateManager>();
}

bool D3d12GraphicsManager::InitializeImpl() {
    bool ok = true;

    auto [w, h] = DisplayManager::GetSingleton().GetWindowSize();
    DEV_ASSERT(w > 0 && h > 0);

    ok = ok && CreateDevice();

    if (m_enableValidationLayer) {
        if (EnableDebugLayer()) {
            LOG("[GraphicsManager_DX12] Debug layer enabled");
        } else {
            LOG_ERROR("[GraphicsManager_DX12] Debug layer not enabled");
        }
    }

    ok = ok && CreateDescriptorHeaps();
    ok = ok && InitGraphicsContext();
    ok = ok && m_copyContext.Initialize(this);

    for (int i = 0; i < NUM_BACK_BUFFERS; i++) {
        auto handle = m_rtvDescHeap.AllocHandle();
        m_renderTargetDescriptor[i] = handle.cpuHandle;
    }

    {
        auto handle = m_dsvDescHeap.AllocHandle();
        m_depthStencilDescriptor = handle.cpuHandle;
    }

    ok = ok && CreateSwapChain(w, h);
    ok = ok && CreateRenderTarget(w, h);
    ok = ok && CreateRootSignature();

    if (!ok) {
        return false;
    }

    // Create debug buffer.
    {
        size_t bufferSize = sizeof(vec4) * 1000;  // hard code
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        D3D_CALL(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_debugVertexData)));
    }
    {
        size_t bufferSize = sizeof(uint32_t) * 3000;  // hard code
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        D3D_CALL(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_debugIndexData)));
    }

    ImGui_ImplDX12_Init(m_device.Get(),
                        NUM_FRAMES_IN_FLIGHT,
                        d3d::Convert(DEFAULT_SURFACE_FORMAT),
                        m_srvDescHeap.GetHeap(),
                        m_srvDescHeap.GetStartCpu(),
                        m_srvDescHeap.GetStartGpu());

    ImGui_ImplDX12_NewFrame();

    SelectRenderGraph();
    return ok;
}

void D3d12GraphicsManager::Finalize() {
    CleanupRenderTarget();

    ImGui_ImplDX12_Shutdown();

    FinalizeGraphicsContext();
    m_copyContext.Finalize();
}

void D3d12GraphicsManager::Render() {
    ID3D12GraphicsCommandList* cmdList = m_graphicsCommandList.Get();

    const auto [width, height] = DisplayManager::GetSingleton().GetWindowSize();
    CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    cmdList->RSSetViewports(1, &viewport);
    D3D12_RECT rect{ 0, 0, width, height };
    cmdList->RSSetScissorRects(1, &rect);

    // bind the frame buffer
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = m_depthStencilDescriptor;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_renderTargetDescriptor[m_backbufferIndex];

    // transfer resource state
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_backbufferIndex],
                                                        D3D12_RESOURCE_STATE_PRESENT,
                                                        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);
    cmdList->ClearRenderTargetView(rtv_handle, DEFAULT_CLEAR_COLOR, 0, nullptr);
    cmdList->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_backbufferIndex],
                                                   D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                   D3D12_RESOURCE_STATE_PRESENT);
    cmdList->ResourceBarrier(1, &barrier);
}

void D3d12GraphicsManager::Present() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    D3D_CALL(m_swapChain->Present(1, 0));  // Present with vsync
}

void D3d12GraphicsManager::BeginFrame() {
    // @TODO: wait for swap chain
    D3d12FrameContext& frame = reinterpret_cast<D3d12FrameContext&>(GetCurrentFrame());
    frame.Wait(m_graphicsFenceEvent, m_graphicsQueueFence.Get());
    D3D_CALL(frame.m_commandAllocator->Reset());
    D3D_CALL(m_graphicsCommandList->Reset(frame.m_commandAllocator, nullptr));

    WaitForSingleObject(m_swapChainWaitObject, INFINITE);

    m_backbufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    m_graphicsCommandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* heap = m_srvDescHeap.GetHeap();
    m_graphicsCommandList->SetDescriptorHeaps(1, &heap);

    // @TODO: NO HARDCODE
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle{ m_srvDescHeap.GetStartGpu() };
    m_graphicsCommandList->SetGraphicsRootDescriptorTable(6, handle);
    handle.Offset(MAX_TEXTURE_2D_COUNT * m_srvDescHeap.GetIncrementSize());
    m_graphicsCommandList->SetGraphicsRootDescriptorTable(7, handle);
}

void D3d12GraphicsManager::EndFrame() {
    D3D_CALL(m_graphicsCommandList->Close());
    ID3D12CommandList* cmdLists[] = { m_graphicsCommandList.Get() };
    m_graphicsCommandQueue->ExecuteCommandLists(array_length(cmdLists), cmdLists);
}

void D3d12GraphicsManager::MoveToNextFrame() {
    uint64_t fenceValue = m_lastSignaledFenceValue + 1;
    m_graphicsCommandQueue->Signal(m_graphicsQueueFence.Get(), fenceValue);
    m_lastSignaledFenceValue = fenceValue;

    D3d12FrameContext& frame = reinterpret_cast<D3d12FrameContext&>(GetCurrentFrame());
    frame.m_fenceValue = fenceValue;
    m_frameIndex = (m_frameIndex + 1) % static_cast<uint32_t>(m_frameContexts.size());
}

std::unique_ptr<FrameContext> D3d12GraphicsManager::CreateFrameContext() {
    return std::make_unique<D3d12FrameContext>();
}

void D3d12GraphicsManager::SetStencilRef(uint32_t p_ref) {
    m_graphicsCommandList->OMSetStencilRef(p_ref);
}

void D3d12GraphicsManager::SetRenderTarget(const DrawPass* p_draw_pass, int p_index, int p_mip_level) {
    unused(p_mip_level);
    DEV_ASSERT(p_draw_pass);

    ID3D12GraphicsCommandList* command_list = m_graphicsCommandList.Get();

    auto draw_pass = reinterpret_cast<const D3d12DrawPass*>(p_draw_pass);
    if (const auto depth_attachment = draw_pass->desc.depthAttachment; depth_attachment) {
        if (depth_attachment->desc.type == AttachmentType::SHADOW_CUBE_ARRAY) {
            D3D12_CPU_DESCRIPTOR_HANDLE dsv{ draw_pass->dsvs[p_index] };
            command_list->OMSetRenderTargets(0, nullptr, false, &dsv);
            return;
        }
    }

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    for (auto& rtv : draw_pass->rtvs) {
        rtvs.emplace_back(rtv);
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE* dsv_ptr = nullptr;
    if (!draw_pass->dsvs.empty()) {
        dsv_ptr = &(draw_pass->dsvs[0]);
    }
    command_list->OMSetRenderTargets((uint32_t)rtvs.size(), rtvs.data(), false, dsv_ptr);
}

void D3d12GraphicsManager::UnsetRenderTarget() {
}

void D3d12GraphicsManager::BeginPass(const RenderPass* p_render_pass) {
    ID3D12GraphicsCommandList* command_list = m_graphicsCommandList.Get();
    for (auto& texture : p_render_pass->GetOutputs()) {
        D3D12_RESOURCE_STATES resource_state{};
        if (texture->desc.bindFlags & BIND_RENDER_TARGET) {
            resource_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        } else if (texture->desc.bindFlags & BIND_DEPTH_STENCIL) {
            resource_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        } else {
            CRASH_NOW();
        }

        auto d3d_texture = reinterpret_cast<D3d12GpuTexture*>(texture.get());
        auto barriers = CD3DX12_RESOURCE_BARRIER::Transition(d3d_texture->texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resource_state);
        command_list->ResourceBarrier(1, &barriers);
    }
}

void D3d12GraphicsManager::EndPass(const RenderPass* p_render_pass) {
    UnsetRenderTarget();
    ID3D12GraphicsCommandList* command_list = m_graphicsCommandList.Get();
    for (auto& texture : p_render_pass->GetOutputs()) {
        D3D12_RESOURCE_STATES resource_state{};
        if (texture->desc.bindFlags & BIND_RENDER_TARGET) {
            resource_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        } else if (texture->desc.bindFlags & BIND_DEPTH_STENCIL) {
            resource_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        } else {
            CRASH_NOW();
        }

        auto d3d_texture = reinterpret_cast<D3d12GpuTexture*>(texture.get());
        auto barriers = CD3DX12_RESOURCE_BARRIER::Transition(d3d_texture->texture.Get(), resource_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list->ResourceBarrier(1, &barriers);
    }
}

void D3d12GraphicsManager::Clear(const DrawPass* p_draw_pass, ClearFlags p_flags, const float* p_clear_color, int p_index) {
    auto draw_pass = reinterpret_cast<const D3d12DrawPass*>(p_draw_pass);

    if (p_flags & CLEAR_COLOR_BIT) {
        DEV_ASSERT(p_clear_color);
        for (auto& rtv : draw_pass->rtvs) {
            m_graphicsCommandList->ClearRenderTargetView(rtv, p_clear_color, 0, nullptr);
        }
    }

    D3D12_CLEAR_FLAGS clear_flags{ static_cast<D3D12_CLEAR_FLAGS>(0) };
    if (p_flags & CLEAR_DEPTH_BIT) {
        clear_flags |= D3D12_CLEAR_FLAG_DEPTH;
    }
    if (p_flags & CLEAR_STENCIL_BIT) {
        clear_flags |= D3D12_CLEAR_FLAG_STENCIL;
    }
    if (clear_flags) {
        // @TODO: better way?
        DEV_ASSERT_INDEX(p_index, draw_pass->dsvs.size());
        m_graphicsCommandList->ClearDepthStencilView(draw_pass->dsvs[p_index], clear_flags, 1.0f, 0, 0, nullptr);
    }
}

void D3d12GraphicsManager::SetViewport(const Viewport& p_viewport) {
    CD3DX12_VIEWPORT viewport(
        static_cast<float>(p_viewport.topLeftX),
        static_cast<float>(p_viewport.topLeftY),
        static_cast<float>(p_viewport.width),
        static_cast<float>(p_viewport.height));

    D3D12_RECT rect{};
    rect.left = 0;
    rect.top = 0;
    rect.right = p_viewport.topLeftX + p_viewport.width;
    rect.bottom = p_viewport.topLeftY + p_viewport.height;

    m_graphicsCommandList->RSSetViewports(1, &viewport);
    m_graphicsCommandList->RSSetScissorRects(1, &rect);
}

const MeshBuffers* D3d12GraphicsManager::CreateMesh(const MeshComponent& p_mesh) {
    auto upload_buffer = [&](uint32_t p_byte_size, const void* p_init_data) -> ID3D12Resource* {
        if (p_byte_size == 0) {
            DEV_ASSERT(p_init_data == nullptr);
            return nullptr;
        }

        ID3D12Resource* buffer = nullptr;
        auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(p_byte_size);
        // Create the actual default buffer resource.
        D3D_FAIL_V(m_device->CreateCommittedResource(
                       &heap_properties,
                       D3D12_HEAP_FLAG_NONE,
                       &buffer_desc,
                       D3D12_RESOURCE_STATE_COMMON,
                       nullptr,
                       IID_PPV_ARGS(&buffer)),
                   nullptr);

        // Describe the data we want to copy into the default buffer.
        D3D12_SUBRESOURCE_DATA sub_resource_data = {};
        sub_resource_data.pData = p_init_data;
        sub_resource_data.RowPitch = p_byte_size;
        sub_resource_data.SlicePitch = sub_resource_data.RowPitch;

        auto cmd = m_copyContext.Allocate(p_byte_size);
        UpdateSubresources<1>(cmd.commandList.Get(), buffer, cmd.uploadBuffer.buffer.Get(), 0, 0, 1, &sub_resource_data);
        m_copyContext.Submit(cmd);
        return buffer;
    };

    RID rid = m_meshes.make_rid();
    D3d12MeshBuffers* mesh_buffers = m_meshes.get_or_null(rid);

#define INIT_BUFFER(INDEX, BUFFER)                                                        \
    do {                                                                                  \
        const uint32_t size_in_byte = static_cast<uint32_t>(VectorSizeInByte(BUFFER));    \
        mesh_buffers->vertexBuffers[INDEX] = upload_buffer(size_in_byte, BUFFER.data());  \
        if (!mesh_buffers->vertexBuffers[INDEX]) {                                        \
            break;                                                                        \
        };                                                                                \
        mesh_buffers->vbvs[INDEX] = {                                                     \
            .BufferLocation = mesh_buffers->vertexBuffers[INDEX]->GetGPUVirtualAddress(), \
            .SizeInBytes = size_in_byte,                                                  \
            .StrideInBytes = sizeof(BUFFER[0]),                                           \
        };                                                                                \
    } while (0)
    INIT_BUFFER(0, p_mesh.positions);
    INIT_BUFFER(1, p_mesh.normals);
    INIT_BUFFER(2, p_mesh.texcoords_0);
    INIT_BUFFER(3, p_mesh.tangents);
    INIT_BUFFER(4, p_mesh.joints_0);
    INIT_BUFFER(5, p_mesh.weights_0);
#undef INIT_BUFFER

    mesh_buffers->indexCount = static_cast<uint32_t>(p_mesh.indices.size());
    mesh_buffers->indexBuffer = upload_buffer(static_cast<uint32_t>(VectorSizeInByte(p_mesh.indices)), p_mesh.indices.data());

    p_mesh.gpuResource = mesh_buffers;
    return mesh_buffers;
}

void D3d12GraphicsManager::SetMesh(const MeshBuffers* p_mesh) {
    auto mesh = reinterpret_cast<const D3d12MeshBuffers*>(p_mesh);

    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = mesh->indexBuffer->GetGPUVirtualAddress();
    ibv.Format = DXGI_FORMAT_R32_UINT;
    ibv.SizeInBytes = mesh->indexCount * sizeof(uint32_t);

    m_graphicsCommandList->IASetVertexBuffers(0, array_length(mesh->vbvs), mesh->vbvs);
    m_graphicsCommandList->IASetIndexBuffer(&ibv);
}

void D3d12GraphicsManager::DrawElements(uint32_t p_count, uint32_t p_offset) {
    m_graphicsCommandList->DrawIndexedInstanced(p_count, 1, p_offset, 0, 0);
}

void D3d12GraphicsManager::DrawElementsInstanced(uint32_t p_instance_count, uint32_t p_count, uint32_t p_offset) {
    m_graphicsCommandList->DrawIndexedInstanced(p_count, p_instance_count, p_offset, 0, 0);
}

// @TODO: remove
WARNING_PUSH()
WARNING_DISABLE(4100, "-Wunused-parameter")
void D3d12GraphicsManager::Dispatch(uint32_t p_num_groups_x, uint32_t p_num_groups_y, uint32_t p_num_groups_z) {
    CRASH_NOW_MSG("Implement");
}

void D3d12GraphicsManager::SetUnorderedAccessView(uint32_t p_slot, GpuTexture* p_texture) {
    CRASH_NOW_MSG("Implement");
}

std::shared_ptr<GpuStructuredBuffer> D3d12GraphicsManager::CreateStructuredBuffer(const GpuBufferDesc& p_desc) {
    // CRASH_NOW_MSG("Implement");
    return nullptr;
}

void D3d12GraphicsManager::BindStructuredBuffer(int p_slot, const GpuStructuredBuffer* p_buffer) {
    CRASH_NOW_MSG("Implement");
}

void D3d12GraphicsManager::UnbindStructuredBuffer(int p_slot) {
    CRASH_NOW_MSG("Implement");
}

void D3d12GraphicsManager::BindStructuredBufferSRV(int p_slot, const GpuStructuredBuffer* p_buffer) {
    CRASH_NOW_MSG("Implement");
}

void D3d12GraphicsManager::UnbindStructuredBufferSRV(int p_slot) {
    CRASH_NOW_MSG("Implement");
}
WARNING_POP()

std::shared_ptr<GpuConstantBuffer> D3d12GraphicsManager::CreateConstantBuffer(const GpuBufferDesc& p_desc) {
    const uint32_t size_in_byte = p_desc.elementCount * p_desc.elementSize;
    CD3DX12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(size_in_byte);
    ComPtr<ID3D12Resource> buffer;
    D3D_FAIL_V(m_device->CreateCommittedResource(
                   &heap_properties,
                   D3D12_HEAP_FLAG_NONE,
                   &buffer_desc,
                   D3D12_RESOURCE_STATE_GENERIC_READ,
                   nullptr, IID_PPV_ARGS(&buffer)),
               nullptr);

    auto result = std::make_shared<D3d12ConstantBuffer>(p_desc);
    result->buffer = buffer;

    D3D_FAIL_V(result->buffer->Map(0, nullptr, reinterpret_cast<void**>(&result->mappedData)), nullptr);

    // @TODO: set debug name
    // SetDebugName(buffer.Get(), "");

    return result;
}

void D3d12GraphicsManager::UpdateConstantBuffer(const GpuConstantBuffer* p_buffer, const void* p_data, size_t p_size) {
    if (p_size) {
        auto cb = reinterpret_cast<const D3d12ConstantBuffer*>(p_buffer);
        memcpy(cb->mappedData, p_data, p_size);
    }
}

void D3d12GraphicsManager::BindConstantBufferRange(const GpuConstantBuffer* p_buffer, uint32_t p_size, uint32_t p_offset) {
    auto buffer = reinterpret_cast<const D3d12ConstantBuffer*>(p_buffer);
    DEV_ASSERT(p_size + p_offset <= buffer->capacity);

    D3D12_GPU_VIRTUAL_ADDRESS batch_address = buffer->buffer->GetGPUVirtualAddress();

    batch_address += p_offset;
    m_graphicsCommandList->SetGraphicsRootConstantBufferView(buffer->desc.slot, batch_address);
}

std::shared_ptr<GpuTexture> D3d12GraphicsManager::CreateTextureImpl(const GpuTextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc) {
    unused(p_sampler_desc);

    auto initial_data = reinterpret_cast<const uint8_t*>(p_texture_desc.initialData);

    bool gen_mip_map = false;
    PixelFormat format = p_texture_desc.format;
    DXGI_FORMAT texture_format = d3d::Convert(format);
    DXGI_FORMAT srv_format = d3d::Convert(format);
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COPY_DEST;

    if (format == PixelFormat::D32_FLOAT) {
        texture_format = DXGI_FORMAT_R32_TYPELESS;
        srv_format = DXGI_FORMAT_R32_FLOAT;
    } else if (format == PixelFormat::D24_UNORM_S8_UINT) {
        texture_format = DXGI_FORMAT_R24G8_TYPELESS;
    } else if (format == PixelFormat::R24G8_TYPELESS) {
        srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    }
    switch (p_texture_desc.type) {
        case AttachmentType::NONE:
            initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
            break;
        default:
            initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            break;
    }

    D3D12_HEAP_PROPERTIES props{};
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    auto ConvertBindFlags = [](BindFlags p_bind_flags) {
        [[maybe_unused]] constexpr BindFlags supported_flags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET | BIND_DEPTH_STENCIL | BIND_UNORDERED_ACCESS;
        DEV_ASSERT((p_bind_flags & (~supported_flags)) == 0);

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        // if (p_bind_flags & BIND_SHADER_RESOURCE) {
        // }
        if (p_bind_flags & BIND_RENDER_TARGET) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if (p_bind_flags & BIND_DEPTH_STENCIL) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if (p_bind_flags & BIND_UNORDERED_ACCESS) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        return flags;
    };

    D3D12_RESOURCE_DESC texture_desc{};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Alignment = 0;
    texture_desc.Width = p_texture_desc.width;
    texture_desc.Height = p_texture_desc.height;
    texture_desc.DepthOrArraySize = static_cast<UINT16>(p_texture_desc.arraySize);
    texture_desc.MipLevels = static_cast<UINT16>(gen_mip_map ? 0 : p_texture_desc.mipLevels);
    texture_desc.Format = texture_format;
    texture_desc.SampleDesc = { 1, 0 };
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = ConvertBindFlags(p_texture_desc.bindFlags);

    ID3D12Resource* texture_ptr = nullptr;
    D3D_FAIL_V(m_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &texture_desc, initial_state, NULL, IID_PPV_ARGS(&texture_ptr)), nullptr);

    if (initial_data) {
        // Create a temporary upload resource to move the data in
        const uint32_t upload_pitch = math::Align(4 * static_cast<int>(p_texture_desc.width), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        const uint32_t upload_size = p_texture_desc.height * upload_pitch;
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        texture_desc.Alignment = 0;
        texture_desc.Width = upload_size;
        texture_desc.Height = 1;
        texture_desc.DepthOrArraySize = 1;
        texture_desc.MipLevels = 1;
        texture_desc.Format = DXGI_FORMAT_UNKNOWN;
        texture_desc.SampleDesc = { 1, 0 };
        texture_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        props.Type = D3D12_HEAP_TYPE_UPLOAD;
        props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        // @TODO: refactor
        ComPtr<ID3D12Resource> upload_buffer;
        D3D_FAIL_V(m_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&upload_buffer)), nullptr);

        // Write pixels into the upload resource
        void* mapped = NULL;
        D3D12_RANGE range = { 0, upload_size };

        upload_buffer->Map(0, &range, &mapped);
        for (uint32_t y = 0; y < p_texture_desc.height; y++) {
            memcpy((void*)((uintptr_t)mapped + y * upload_pitch), initial_data + y * p_texture_desc.width * 4, p_texture_desc.width * 4);
        }
        upload_buffer->Unmap(0, &range);

        // Copy the upload resource content into the real resource
        D3D12_TEXTURE_COPY_LOCATION source_location = {};
        source_location.pResource = upload_buffer.Get();
        source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        source_location.PlacedFootprint.Footprint.Width = p_texture_desc.width;
        source_location.PlacedFootprint.Footprint.Height = p_texture_desc.height;
        source_location.PlacedFootprint.Footprint.Depth = 1;
        source_location.PlacedFootprint.Footprint.RowPitch = upload_pitch;

        D3D12_TEXTURE_COPY_LOCATION dest_location = {};
        dest_location.pResource = texture_ptr;
        dest_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest_location.SubresourceIndex = 0;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texture_ptr;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // Create a temporary command queue to do the copy with
        ComPtr<ID3D12Fence> fence;
        D3D_FAIL_V(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), nullptr);

        HANDLE event = CreateEvent(0, 0, 0, 0);
        if (event == NULL) {
            return nullptr;
        }

        D3D12_COMMAND_QUEUE_DESC queue_desc = {};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.NodeMask = 1;

        ComPtr<ID3D12CommandQueue> copy_queue;
        D3D_FAIL_V(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&copy_queue)), nullptr);

        ComPtr<ID3D12CommandAllocator> copy_alloc;
        D3D_FAIL_V(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&copy_alloc)), nullptr);

        ComPtr<ID3D12GraphicsCommandList> command_list;
        D3D_FAIL_V(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, copy_alloc.Get(), NULL, IID_PPV_ARGS(&command_list)), nullptr);

        command_list->CopyTextureRegion(&dest_location, 0, 0, 0, &source_location, NULL);
        command_list->ResourceBarrier(1, &barrier);
        command_list->Close();

        // Execute the copy
        ID3D12CommandList* command_lists[] = { command_list.Get() };
        copy_queue->ExecuteCommandLists(array_length(command_lists), command_lists);
        copy_queue->Signal(fence.Get(), 1);

        // Wait for everything to complete
        fence->SetEventOnCompletion(1, event);
        WaitForSingleObject(event, INFINITE);

        // Tear down our temporary command queue and release the upload resource
        CloseHandle(event);
    }

    DescriptorHeap::Handle handle = m_srvDescHeap.AllocHandle(p_texture_desc.dimension);

    // Create a shader resource view for the texture
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    if (p_texture_desc.bindFlags & BIND_SHADER_RESOURCE) {
        srv_desc.Format = srv_format;
        switch (p_texture_desc.dimension) {
            case Dimension::TEXTURE_2D:
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;
                srv_desc.Texture2D.MostDetailedMip = 0;
                break;
            case Dimension::TEXTURE_CUBE_ARRAY:
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                srv_desc.TextureCubeArray.MipLevels = texture_desc.MipLevels;
                srv_desc.TextureCubeArray.MostDetailedMip = 0;
                srv_desc.TextureCubeArray.First2DArrayFace = 0;
                srv_desc.TextureCubeArray.NumCubes = p_texture_desc.arraySize / 6;
                break;
            default:
                CRASH_NOW();
                break;
        }
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        m_device->CreateShaderResourceView(texture_ptr, &srv_desc, handle.cpuHandle);
    }

    auto gpu_texture = std::make_shared<D3d12GpuTexture>(p_texture_desc);
    gpu_texture->index = handle.index;
    gpu_texture->cpuHandle = handle.cpuHandle.ptr;
    gpu_texture->gpuHandle = handle.gpuHandle.ptr;
    gpu_texture->texture = ComPtr<ID3D12Resource>(texture_ptr);
    SetDebugName(texture_ptr, RenderTargetResourceNameToString(p_texture_desc.name));
    return gpu_texture;
}

void D3d12GraphicsManager::BindTexture(Dimension p_dimension, uint64_t p_handle, int p_slot) {
    unused(p_dimension);
    unused(p_handle);
    unused(p_slot);
}

void D3d12GraphicsManager::UnbindTexture(Dimension p_dimension, int p_slot) {
    unused(p_dimension);
    unused(p_slot);
}

std::shared_ptr<DrawPass> D3d12GraphicsManager::CreateDrawPass(const DrawPassDesc& p_subpass_desc) {
    auto draw_pass = std::make_shared<D3d12DrawPass>(p_subpass_desc);

    for (const auto& color_attachment : p_subpass_desc.colorAttachments) {
        auto texture = reinterpret_cast<const D3d12GpuTexture*>(color_attachment.get());
        switch (color_attachment->desc.type) {
            case AttachmentType::COLOR_2D: {
                auto handle = m_rtvDescHeap.AllocHandle();
                m_device->CreateRenderTargetView(texture->texture.Get(), nullptr, handle.cpuHandle);
                draw_pass->rtvs.emplace_back(handle.cpuHandle);
            } break;
            default:
                CRASH_NOW();
                break;
        }
    }

    if (auto& depth_attachment = draw_pass->desc.depthAttachment; depth_attachment) {
        auto texture = reinterpret_cast<const D3d12GpuTexture*>(depth_attachment.get());
        switch (depth_attachment->desc.type) {
            case AttachmentType::DEPTH_2D: {
                // ComPtr<ID3D11DepthStencilView> dsv;
                // D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
                // desc.Format = DXGI_FORMAT_D32_FLOAT;
                // desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                // desc.Texture2D.MipSlice = 0;

                // D3D_FAIL_V_MSG(m_device->CreateDepthStencilView(texture->texture.Get(), &desc, dsv.GetAddressOf()),
                //                nullptr,
                //                "Failed to create depth stencil view");
                // draw_pass->dsvs.push_back(dsv);
                CRASH_NOW();
            } break;
            case AttachmentType::DEPTH_STENCIL_2D: {
                D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsv_desc.Texture2D.MipSlice = 0;
                auto handle = m_dsvDescHeap.AllocHandle();
                m_device->CreateDepthStencilView(texture->texture.Get(), &dsv_desc, handle.cpuHandle);
                draw_pass->dsvs.emplace_back(handle.cpuHandle);
            } break;
            case AttachmentType::SHADOW_2D: {
                D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsv_desc.Texture2D.MipSlice = 0;

                auto handle = m_dsvDescHeap.AllocHandle();
                m_device->CreateDepthStencilView(texture->texture.Get(), &dsv_desc, handle.cpuHandle);
                draw_pass->dsvs.emplace_back(handle.cpuHandle);
            } break;
            case AttachmentType::SHADOW_CUBE_ARRAY: {
                for (uint32_t face = 0; face < depth_attachment->desc.arraySize; ++face) {
                    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                    dsv_desc.Texture2DArray.MipSlice = 0;
                    dsv_desc.Texture2DArray.ArraySize = 1;
                    dsv_desc.Texture2DArray.FirstArraySlice = face;

                    auto handle = m_dsvDescHeap.AllocHandle();
                    m_device->CreateDepthStencilView(texture->texture.Get(), &dsv_desc, handle.cpuHandle);
                    draw_pass->dsvs.emplace_back(handle.cpuHandle);
                }
            } break;
            default:
                CRASH_NOW();
                break;
        }
    }

    return draw_pass;
}

bool D3d12GraphicsManager::CreateDevice() {
    if (m_enableValidationLayer) {
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)))) {
            m_debugController->EnableDebugLayer();
        }
    }

    D3D_FAIL_V(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)), false);

    auto get_hardware_adapter = [](IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter) {
        *ppAdapter = nullptr;
        for (UINT adapter_index = 0;; ++adapter_index) {
            IDXGIAdapter1* adapter = nullptr;
            if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapter_index, &adapter)) {
                // No more adapters to enumerate.
                break;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr))) {
                *ppAdapter = adapter;
                return;
            }
            adapter->Release();
        }
    };

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    get_hardware_adapter(m_factory.Get(), &hardwareAdapter);

    if (FAILED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)))) {
        ComPtr<IDXGIAdapter> warpAdapter;
        D3D_FAIL_V(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)), false);

        D3D_FAIL_V(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)), false);
    }

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels = { array_length(featureLevels), featureLevels, D3D_FEATURE_LEVEL_12_0 };

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
    D3D_CALL(m_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels)));
    featureLevel = featLevels.MaxSupportedFeatureLevel;
    switch (featureLevel) {
        case D3D_FEATURE_LEVEL_12_0:
            LOG("[DX12] Device Feature Level: 12.0");
            break;
        case D3D_FEATURE_LEVEL_12_1:
            LOG("[DX12] Device Feature Level: 12.1");
            break;
        default:
            CRASH_NOW();
            break;
    }

    return true;
}

bool D3d12GraphicsManager::InitGraphicsContext() {
    m_graphicsCommandQueue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DEV_ASSERT(m_graphicsCommandQueue);

    int frame_idx = 0;
    for (auto& it : m_frameContexts) {
        D3d12FrameContext& frame = reinterpret_cast<D3d12FrameContext&>(*it.get());
        D3D_FAIL_V(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.m_commandAllocator)), false);
        D3D12_SET_DEBUG_NAME(frame.m_commandAllocator, std::format("GraphicsCommandAllocator {}", frame_idx++));

        if (m_graphicsCommandList == nullptr) {
            D3D_FAIL_V(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.m_commandAllocator, nullptr, IID_PPV_ARGS(&m_graphicsCommandList)), false);
        }
    }

    m_graphicsCommandList->Close();
    D3D12_SET_DEBUG_NAME(m_graphicsCommandList.Get(), "GraphicsCommandList");

    D3D_FAIL_V(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_graphicsQueueFence)), false);

    D3D12_SET_DEBUG_NAME(m_graphicsQueueFence.Get(), "GraphicsFence");

    m_graphicsFenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

    return true;
}

void D3d12GraphicsManager::FinalizeGraphicsContext() {
    FlushGraphicsContext();

    m_graphicsQueueFence.Reset();
    m_lastSignaledFenceValue = 0;

    CloseHandle(m_graphicsFenceEvent);
    m_graphicsFenceEvent = NULL;

    m_graphicsCommandList.Reset();
    m_graphicsCommandQueue.Reset();

    m_frameContexts.clear();
}

void D3d12GraphicsManager::FlushGraphicsContext() {
    for (auto& it : m_frameContexts) {
        D3d12FrameContext& frame = reinterpret_cast<D3d12FrameContext&>(*it.get());
        frame.Wait(m_graphicsFenceEvent, m_graphicsQueueFence.Get());
    }
    m_frameIndex = 0;
}

ID3D12CommandQueue* D3d12GraphicsManager::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE p_type) {
    D3D12_COMMAND_QUEUE_DESC desc;
    desc.Type = p_type;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    ID3D12CommandQueue* queue;

    D3D_CALL(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)));

#if USING(USE_D3D_DEBUG_NAME)
    switch (p_type) {
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            D3D12_SET_DEBUG_NAME(queue, "GraphicsCommandQueue");
            break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            D3D12_SET_DEBUG_NAME(queue, "ComputeCommandQueue");
            break;
        case D3D12_COMMAND_LIST_TYPE_COPY:
            D3D12_SET_DEBUG_NAME(queue, "CopyCommandQueue");
            break;
        default:
            CRASH_NOW();
            break;
    }
#endif

    return queue;
}

bool D3d12GraphicsManager::EnableDebugLayer() {
    D3D_FAIL_V(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)), false);

    m_debugController->EnableDebugLayer();

    ComPtr<ID3D12Debug> debug_device;
    m_device->QueryInterface(IID_PPV_ARGS(debug_device.GetAddressOf()));

    ComPtr<ID3D12InfoQueue> info_queue;

    D3D_FAIL_V(m_device->QueryInterface(IID_PPV_ARGS(&info_queue)), false);

    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);

    D3D12_MESSAGE_ID ignore_list[] = {
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
        D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
        // D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES
    };

    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.pIDList = ignore_list;
    filter.DenyList.NumIDs = array_length(ignore_list);
    info_queue->AddRetrievalFilterEntries(&filter);
    info_queue->AddStorageFilterEntries(&filter);

    return true;
}

bool D3d12GraphicsManager::CreateDescriptorHeaps() {
    bool ok = true;
    ok = ok && m_rtvDescHeap.Initialize(0, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, m_device.Get(), false);
    ok = ok && m_dsvDescHeap.Initialize(0, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, m_device.Get(), false);
    // reserve one srv for ImGui
    ok = ok && m_srvDescHeap.Initialize(1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 512, m_device.Get(), true);
    return ok;
}

bool D3d12GraphicsManager::CreateSwapChain(uint32_t p_width, uint32_t p_height) {
    auto display_manager = dynamic_cast<Win32DisplayManager*>(DisplayManager::GetSingletonPtr());
    DEV_ASSERT(display_manager);

    // create a struct to hold information about the swap chain
    DXGI_SWAP_CHAIN_DESC1 scd{};

    // fill the swap chain description struct
    scd.Width = p_width;
    scd.Height = p_height;
    scd.Format = d3d::Convert(DEFAULT_SURFACE_FORMAT);
    scd.Stereo = FALSE;
    scd.SampleDesc = { 1, 0 };
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // how swap chain is to be used
    scd.BufferCount = NUM_FRAMES_IN_FLIGHT;             // back buffer count
    scd.Scaling = DXGI_SCALING_STRETCH;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    IDXGISwapChain1* pSwapChain = nullptr;

    HRESULT hr = m_factory->CreateSwapChainForHwnd(
        m_graphicsCommandQueue.Get(),
        display_manager->GetHwnd(),
        &scd,
        NULL,
        NULL,
        &pSwapChain);

    D3D_FAIL_V_MSG(hr, false, "Failed to create swapchain");

    m_swapChain.Attach(reinterpret_cast<IDXGISwapChain3*>(pSwapChain));

    m_swapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
    m_swapChainWaitObject = m_swapChain->GetFrameLatencyWaitableObject();

    return true;
}

bool D3d12GraphicsManager::CreateRenderTarget(uint32_t p_width, uint32_t p_height) {
    for (int32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; i++) {
        ID3D12Resource* pBackBuffer = nullptr;
        D3D_CALL(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
        m_device->CreateRenderTargetView(pBackBuffer, nullptr, m_renderTargetDescriptor[i]);
        std::wstring name = std::wstring(L"Render Target Buffer") + std::to_wstring(i);
        pBackBuffer->SetName(name.c_str());
        m_renderTargets[i] = pBackBuffer;
    }

    D3D12_CLEAR_VALUE depthOptimizedClearValue{};
    depthOptimizedClearValue.Format = d3d::Convert(DEFAULT_DEPTH_STENCIL_FORMAT);
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES prop{};
    prop.Type = D3D12_HEAP_TYPE_DEFAULT;
    prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    prop.CreationNodeMask = 1;
    prop.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Alignment = 0;
    resource_desc.Width = p_width;
    resource_desc.Height = p_height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = d3d::Convert(DEFAULT_DEPTH_STENCIL_FORMAT);
    resource_desc.SampleDesc = { 1, 0 };
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    HRESULT hr = m_device->CreateCommittedResource(
        &prop, D3D12_HEAP_FLAG_NONE, &resource_desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue,
        IID_PPV_ARGS(&m_depthStencilBuffer));

    D3D_FAIL_V_MSG(hr, false, "Failed to create committed resource");

    m_depthStencilBuffer->SetName(L"Depth Stencil Buffer");
    m_device->CreateDepthStencilView(m_depthStencilBuffer, nullptr, m_depthStencilDescriptor);

    return true;
}

void D3d12GraphicsManager::InitStaticSamplers() {
    auto FillSamplerDesc = [](uint32_t p_slot, const SamplerDesc& p_desc) {
        CD3DX12_STATIC_SAMPLER_DESC sampler_desc(
            p_slot,
            d3d::Convert(p_desc.minFilter, p_desc.magFilter),
            d3d::Convert(p_desc.addressU),
            d3d::Convert(p_desc.addressV),
            d3d::Convert(p_desc.addressW),
            p_desc.mipLodBias,
            p_desc.maxAnisotropy,
            d3d::Convert(p_desc.comparisonFunc),
            d3d::Convert(p_desc.staticBorderColor),
            p_desc.minLod,
            p_desc.maxLod);

        return sampler_desc;
    };

#define SAMPLER_STATE(REG, NAME, DESC) m_staticSamplers.emplace_back(FillSamplerDesc(REG, DESC));
#include "sampler.hlsl.h"
#undef SAMPLER_STATE
}

bool D3d12GraphicsManager::CreateRootSignature() {
    // Create a root signature consisting of a descriptor table with a single CBV.

    CD3DX12_DESCRIPTOR_RANGE texture2d_range(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,  // type
        MAX_TEXTURE_2D_COUNT,             // number of descriptors
        0,                                // register t0
        0);                               // space 0
    CD3DX12_DESCRIPTOR_RANGE texture_cube_array_range(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,  // type
        MAX_TEXTURE_CUBE_ARRAY_COUNT,     // number of descriptors
        0,                                // register t0
        1);                               // space 1

    // TODO: Order from most frequent to least frequent.
    CD3DX12_ROOT_PARAMETER root_parameters[16]{};
    int param_count = 0;

    // @TODO: fix this
    root_parameters[param_count++].InitAsConstantBufferView(0);
    root_parameters[param_count++].InitAsConstantBufferView(1);
    root_parameters[param_count++].InitAsConstantBufferView(2);
    root_parameters[param_count++].InitAsConstantBufferView(3);
    root_parameters[param_count++].InitAsConstantBufferView(4);
    root_parameters[param_count++].InitAsConstantBufferView(5);

    root_parameters[param_count++].InitAsDescriptorTable(1, &texture2d_range);
    root_parameters[param_count++].InitAsDescriptorTable(1, &texture_cube_array_range);

    InitStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc(param_count,
                                                    root_parameters,
                                                    (uint32_t)m_staticSamplers.size(),
                                                    m_staticSamplers.data(),
                                                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    // @TODO: print error
    HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        char buffer[256]{ 0 };
        StringUtils::Sprintf(buffer, "%.*s", error->GetBufferSize(), error->GetBufferPointer());
        LOG_ERROR("Failed to create root signature, reason {}", buffer);
        return false;
    }

    D3D_FAIL_V(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)), false);

    return true;
}

void D3d12GraphicsManager::OnSceneChange(const Scene& p_scene) {
    for (auto [entity, mesh] : p_scene.m_MeshComponents) {
        if (mesh.gpuResource != nullptr) {
            const NameComponent& name = *p_scene.GetComponent<NameComponent>(entity);
            LOG_WARN("[begin_scene] mesh '{}' () already has gpu resource", name.GetName());
            continue;
        }

        CreateMesh(mesh);
    }
}

void D3d12GraphicsManager::OnWindowResize(int p_width, int p_height) {
    if (m_swapChain) {
        CleanupRenderTarget();
        D3D_CALL(m_swapChain->ResizeBuffers(0, p_width, p_height,
                                            DXGI_FORMAT_UNKNOWN,
                                            DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

        [[maybe_unused]] auto err = CreateRenderTarget(p_width, p_height);
    }
}

void D3d12GraphicsManager::SetPipelineStateImpl(PipelineStateName p_name) {
    // @TODO: refactor topology
    m_graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto pipeline = reinterpret_cast<D3d12PipelineState*>(m_pipelineStateManager->Find(p_name));
    DEV_ASSERT(pipeline);
    m_graphicsCommandList->SetPipelineState(pipeline->pso.Get());
}

void D3d12GraphicsManager::CleanupRenderTarget() {
    FlushGraphicsContext();

    for (uint32_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
        SafeRelease(m_renderTargets[i]);
    }
    SafeRelease(m_depthStencilBuffer);
}

}  // namespace my
