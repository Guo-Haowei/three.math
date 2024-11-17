#pragma once
#include "core/base/rid_owner.h"
#include "core/framework/graphics_manager.h"
#include "drivers/d3d12/d3d12_core.h"

namespace my {

struct D3d12MeshBuffers : public MeshBuffers {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffers[6]{};
    D3D12_VERTEX_BUFFER_VIEW vbvs[6]{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    uint32_t indexBufferByte{ 0 };
};

class D3d12GraphicsManager : public GraphicsManager {
public:
    D3d12GraphicsManager();

    void Finalize() final;

    void SetStencilRef(uint32_t p_ref) final;

    void SetRenderTarget(const DrawPass* p_draw_pass, int p_index, int p_mip_level) final;
    void UnsetRenderTarget() final;
    void BeginPass(const RenderPass* p_render_pass) final;
    void EndPass(const RenderPass* p_render_pass) final;

    void Clear(const DrawPass* p_draw_pass, ClearFlags p_flags, const float* p_clear_color, int p_index) final;
    void SetViewport(const Viewport& p_viewport) final;

    const MeshBuffers* CreateMesh(const MeshComponent& p_mesh) final;
    void SetMesh(const MeshBuffers* p_mesh) final;
    void DrawElements(uint32_t p_count, uint32_t p_offset) final;
    void DrawElementsInstanced(uint32_t p_instance_count, uint32_t p_count, uint32_t p_offset) final;

    void Dispatch(uint32_t p_num_groups_x, uint32_t p_num_groups_y, uint32_t p_num_groups_z) final;
    void SetUnorderedAccessView(uint32_t p_slot, GpuTexture* p_texture) final;

    std::shared_ptr<GpuStructuredBuffer> CreateStructuredBuffer(const GpuBufferDesc& p_desc) final;
    void BindStructuredBuffer(int p_slot, const GpuStructuredBuffer* p_buffer) final;
    void UnbindStructuredBuffer(int p_slot) final;
    void BindStructuredBufferSRV(int p_slot, const GpuStructuredBuffer* p_buffer) final;
    void UnbindStructuredBufferSRV(int p_slot) final;

    std::shared_ptr<GpuConstantBuffer> CreateConstantBuffer(const GpuBufferDesc& p_desc) final;
    void UpdateConstantBuffer(const GpuConstantBuffer* p_buffer, const void* p_data, size_t p_size) final;
    void BindConstantBufferRange(const GpuConstantBuffer* p_buffer, uint32_t p_size, uint32_t p_offset) final;

    // @TODO: remove Dimension
    void BindTexture(Dimension p_dimension, uint64_t p_handle, int p_slot) final;
    void UnbindTexture(Dimension p_dimension, int p_slot) final;

    std::shared_ptr<DrawPass> CreateDrawPass(const DrawPassDesc& p_subpass_desc) final;

    ID3D12CommandQueue* CreateCommandQueue(D3D12_COMMAND_LIST_TYPE p_type);

    ID3D12Device4* const GetDevice() const { return m_device.Get(); }
    ID3D12RootSignature* const GetRootSignature() const { return m_rootSignature.Get(); }

protected:
    bool InitializeImpl() final;
    std::shared_ptr<GpuTexture> CreateGpuTextureImpl(const GpuTextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc) final;

    void Render() final;
    void Present() final;

    void BeginFrame() final;
    void EndFrame() final;
    void MoveToNextFrame() final;
    std::unique_ptr<FrameContext> CreateFrameContext() final;

    void OnSceneChange(const Scene& p_scene) final;
    void OnWindowResize(int p_width, int p_height) final;
    void SetPipelineStateImpl(PipelineStateName p_name) final;

private:
    bool CreateDevice();
    bool InitGraphicsContext();
    void FinalizeGraphicsContext();
    void FlushGraphicsContext();

    bool EnableDebugLayer();
    bool CreateDescriptorHeaps();
    bool CreateRootSignature();
    bool CreateSwapChain(uint32_t p_width, uint32_t p_height);
    bool CreateRenderTarget(uint32_t p_width, uint32_t p_height);
    void CleanupRenderTarget();
    void InitStaticSamplers();

    DescriptorHeapGPU m_rtvDescHeap;
    DescriptorHeapGPU m_dsvDescHeap;
    DescriptorHeapGPU m_srvDescHeap;

    Microsoft::WRL::ComPtr<ID3D12Device4> m_device;
    Microsoft::WRL::ComPtr<ID3D12Debug> m_debugController;
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

    // Graphics Queue
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_graphicsCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;
    Microsoft::WRL::ComPtr<ID3D12Fence1> m_graphicsQueueFence;

    uint64_t m_lastSignaledFenceValue = 0;
    HANDLE m_graphicsFenceEvent = NULL;

    // Copy Queue
    CopyContext m_copyContext;

    HANDLE m_swapChainWaitObject = INVALID_HANDLE_VALUE;

    // Render Target

    ID3D12Resource* m_renderTargets[NUM_BACK_BUFFERS] = { nullptr };
    D3D12_CPU_DESCRIPTOR_HANDLE m_renderTargetDescriptor[NUM_BACK_BUFFERS] = {};
    ID3D12Resource* m_depthStencilBuffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_depthStencilDescriptor = {};

    uint32_t m_backbufferIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_debugVertexData;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_debugIndexData;
    std::vector<CD3DX12_STATIC_SAMPLER_DESC> m_staticSamplers;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textures;

    RIDAllocator<D3d12MeshBuffers> m_meshes;
};

}  // namespace my
