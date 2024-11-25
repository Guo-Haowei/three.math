#pragma once
#include "core/base/concurrent_queue.h"
#include "core/base/singleton.h"
#include "core/framework/event_queue.h"
#include "core/framework/module.h"
#include "core/framework/pipeline_state_manager.h"
#include "rendering/gpu_resource.h"
#include "rendering/pipeline_state.h"
#include "rendering/render_graph/draw_pass.h"
#include "rendering/render_graph/render_graph.h"
#include "rendering/render_graph/render_pass.h"
#include "rendering/sampler.h"
#include "scene/material_component.h"

// @TODO: refactor
#include "cbuffer.hlsl.h"
#include "scene/scene.h"
struct MaterialConstantBuffer;
using my::rg::RenderPass;

namespace my {

// @TODO: refactor
extern ConstantBuffer<PerSceneConstantBuffer> g_constantCache;
extern ConstantBuffer<DebugDrawConstantBuffer> g_debug_draw_cache;
extern ConstantBuffer<EnvConstantBuffer> g_env_cache;

// @TODO: refactor
struct DrawContext {
    uint32_t index_count;
    uint32_t index_offset;
    int material_idx;
};

// @TODO: refactor
struct BatchContext {
    int bone_idx;
    int batch_idx;
    const MeshBuffers* mesh_data;
    std::vector<DrawContext> subsets;
    uint32_t flags;
};

struct PassContext {
    int pass_idx{ 0 };

    std::vector<BatchContext> draws;
};

struct FrameContext {
    template<typename BUFFER>
    struct BufferCache {
        std::vector<BUFFER> buffer;
        std::unordered_map<ecs::Entity, uint32_t> lookup;

        uint32_t FindOrAdd(ecs::Entity p_entity, const BUFFER& p_buffer) {
            auto it = lookup.find(p_entity);
            if (it != lookup.end()) {
                return it->second;
            }

            uint32_t index = static_cast<uint32_t>(buffer.size());
            lookup[p_entity] = index;
            buffer.emplace_back(p_buffer);
            return index;
        }

        void Clear() {
            buffer.clear();
            lookup.clear();
        }
    };

    std::shared_ptr<GpuConstantBuffer> batchCb;
    BufferCache<PerBatchConstantBuffer> batchCache;

    std::shared_ptr<GpuConstantBuffer> materialCb;
    BufferCache<MaterialConstantBuffer> materialCache;

    std::shared_ptr<GpuConstantBuffer> boneCb;
    BufferCache<BoneConstantBuffer> boneCache;

    std::shared_ptr<GpuConstantBuffer> passCb;
    std::vector<PerPassConstantBuffer> passCache;

    std::shared_ptr<GpuConstantBuffer> emitterCb;
    std::vector<EmitterConstantBuffer> emitterCache;

    std::shared_ptr<GpuConstantBuffer> pointShadowCb;
    std::array<PointShadowConstantBuffer, MAX_POINT_LIGHT_SHADOW_COUNT * 6> pointShadowCache;

    std::shared_ptr<GpuConstantBuffer> perFrameCb;
    PerFrameConstantBuffer perFrameCache;
};

enum class RenderGraphName : uint8_t {
    DEFAULT = 0,
    DUMMY,
    EXPERIMENTAL,
    PATHTRACER,
    COUNT,
};

class GraphicsManager : public Singleton<GraphicsManager>, public Module, public EventListener {
public:
    static constexpr int NUM_FRAMES_IN_FLIGHT = 2;
    static constexpr int NUM_BACK_BUFFERS = 2;
    static constexpr float DEFAULT_CLEAR_COLOR[4] = { 0.0f, 0.0f, 0.0f, 1.0 };
    static constexpr PixelFormat DEFAULT_SURFACE_FORMAT = PixelFormat::R8G8B8A8_UNORM;
    static constexpr PixelFormat DEFAULT_DEPTH_STENCIL_FORMAT = PixelFormat::D32_FLOAT;

    using OnTextureLoadFunc = void (*)(Image* p_image);

    GraphicsManager(std::string_view p_name, Backend p_backend, int p_frame_count)
        : Module(p_name),
          m_backend(p_backend),
          m_frameCount(p_frame_count) {}

    auto Initialize() -> Result<void> final;
    void Update(Scene& p_scene);

    // resource
    virtual auto CreateConstantBuffer(const GpuBufferDesc& p_desc) -> Result<std::shared_ptr<GpuConstantBuffer>> = 0;
    virtual auto CreateStructuredBuffer(const GpuBufferDesc& p_desc) -> Result<std::shared_ptr<GpuStructuredBuffer>> = 0;

    virtual void SetRenderTarget(const DrawPass* p_draw_pass, int p_index = 0, int p_mip_level = 0) = 0;
    virtual void UnsetRenderTarget() = 0;
    virtual void BeginDrawPass(const DrawPass* p_draw_pass);
    virtual void EndDrawPass(const DrawPass* p_draw_pass);

    virtual void Clear(const DrawPass* p_draw_pass, ClearFlags p_flags, const float* p_clear_color = DEFAULT_CLEAR_COLOR, int p_index = 0) = 0;
    virtual void SetViewport(const Viewport& p_viewport) = 0;

    virtual const MeshBuffers* CreateMesh(const MeshComponent& p_mesh) = 0;
    virtual void SetMesh(const MeshBuffers* p_mesh) = 0;
    virtual void DrawElements(uint32_t p_count, uint32_t p_offset = 0) = 0;
    virtual void DrawElementsInstanced(uint32_t p_instance_count, uint32_t p_count, uint32_t p_offset = 0) = 0;

    virtual void Dispatch(uint32_t p_num_groups_x, uint32_t p_num_groups_y, uint32_t p_num_groups_z) = 0;
    virtual void BindUnorderedAccessView(uint32_t p_slot, GpuTexture* p_texture) = 0;
    virtual void UnbindUnorderedAccessView(uint32_t p_slot) = 0;

    void SetPipelineState(PipelineStateName p_name);

    virtual void SetStencilRef(uint32_t p_ref) = 0;
    virtual void SetBlendState(const BlendDesc& p_desc, const float* p_factor, uint32_t p_mask) = 0;

    virtual void BindStructuredBuffer(int p_slot, const GpuStructuredBuffer* p_buffer) = 0;
    virtual void UnbindStructuredBuffer(int p_slot) = 0;
    virtual void BindStructuredBufferSRV(int p_slot, const GpuStructuredBuffer* p_buffer) = 0;
    virtual void UnbindStructuredBufferSRV(int p_slot) = 0;

    virtual void UpdateConstantBuffer(const GpuConstantBuffer* p_buffer, const void* p_data, size_t p_size) = 0;
    template<typename T>
    void UpdateConstantBuffer(const GpuConstantBuffer* p_buffer, const std::vector<T>& p_vector) {
        UpdateConstantBuffer(p_buffer, p_vector.data(), sizeof(T) * (uint32_t)p_vector.size());
    }
    template<typename T, int N>
    void UpdateConstantBuffer(const GpuConstantBuffer* p_buffer, const std::array<T, N>& p_array) {
        UpdateConstantBuffer(p_buffer, p_array.data(), sizeof(T) * N);
    }

    virtual void BindConstantBufferRange(const GpuConstantBuffer* p_buffer, uint32_t p_size, uint32_t p_offset) = 0;
    template<typename T>
    void BindConstantBufferSlot(const GpuConstantBuffer* p_buffer, int slot) {
        BindConstantBufferRange(p_buffer, sizeof(T), slot * sizeof(T));
    }

    virtual std::shared_ptr<DrawPass> CreateDrawPass(const DrawPassDesc& p_desc) = 0;

    std::shared_ptr<GpuTexture> CreateTexture(const GpuTextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc);
    std::shared_ptr<GpuTexture> FindTexture(RenderTargetResourceName p_name) const;
    virtual void BindTexture(Dimension p_dimension, uint64_t p_handle, int p_slot) = 0;
    virtual void UnbindTexture(Dimension p_dimension, int p_slot) = 0;

    virtual void GenerateMipmap(const GpuTexture* p_texture) = 0;

    void RequestTexture(ImageHandle* p_handle, OnTextureLoadFunc p_func = nullptr);

    // @TODO: move to renderer
    uint64_t GetFinalImage() const;

    // @TODO: thread safety ?
    void EventReceived(std::shared_ptr<IEvent> p_event) final;

    static auto Create() -> Result<std::shared_ptr<GraphicsManager>>;

    Backend GetBackend() const { return m_backend; }

    [[nodiscard]] auto SelectRenderGraph() -> Result<void>;
    RenderGraphName GetActiveRenderGraphName() const { return m_activeRenderGraphName; }
    void SetActiveRenderGraph(RenderGraphName p_name);
    rg::RenderGraph* GetActiveRenderGraph();
    const auto& GetRenderGraphs() const { return m_renderGraphs; }

    FrameContext& GetCurrentFrame() { return *(m_frameContexts[m_frameIndex].get()); }

protected:
    virtual auto InitializeImpl() -> Result<void> = 0;
    virtual std::shared_ptr<GpuTexture> CreateTextureImpl(const GpuTextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc) = 0;

    virtual void Render() = 0;
    virtual void Present() = 0;

    virtual void BeginFrame();
    virtual void EndFrame();
    virtual void MoveToNextFrame();
    virtual std::unique_ptr<FrameContext> CreateFrameContext();

    virtual void OnSceneChange(const Scene& p_scene) = 0;
    virtual void OnWindowResize(int p_width, int p_height) = 0;
    virtual void SetPipelineStateImpl(PipelineStateName p_name) = 0;

    const Backend m_backend;
    RenderGraphName m_activeRenderGraphName{ RenderGraphName::DEFAULT };
    bool m_enableValidationLayer;

    std::array<std::unique_ptr<rg::RenderGraph>, std::to_underlying(RenderGraphName::COUNT)> m_renderGraphs;

    std::map<RenderTargetResourceName, std::shared_ptr<GpuTexture>> m_resourceLookup;

    struct ImageTask {
        ImageHandle* handle;
        OnTextureLoadFunc func;
    };
    ConcurrentQueue<ImageTask> m_loadedImages;

    std::shared_ptr<PipelineStateManager> m_pipelineStateManager;
    std::vector<std::unique_ptr<FrameContext>> m_frameContexts;
    int m_frameIndex{ 0 };
    const int m_frameCount;

public:
    using FilterObjectFunc1 = std::function<bool(const ObjectComponent& p_object)>;
    using FilterObjectFunc2 = std::function<bool(const AABB& p_object_aabb)>;

    // @TODO: save pass item somewhere and use index instead of keeping many copies
    std::array<std::unique_ptr<PassContext>, MAX_POINT_LIGHT_SHADOW_COUNT> m_pointShadowPasses;
    std::array<PassContext, 1> m_shadowPasses;  // @TODO: support multi ortho light

    PassContext m_voxelPass;
    PassContext m_mainPass;

    // @TODO: refactor
    std::shared_ptr<GpuStructuredBuffer> m_pathTracerBvhBuffer;
    std::shared_ptr<GpuStructuredBuffer> m_pathTracerGeometryBuffer;
    std::shared_ptr<GpuStructuredBuffer> m_pathTracerMaterialBuffer;

protected:
    void Cleanup();
    void UpdateConstants(const Scene& p_scene);
    void UpdateEmitters(const Scene& p_scene);
    void UpdateForceFields(const Scene& p_scene);
    void UpdateLights(const Scene& p_scene);
    void UpdateVoxelPass(const Scene& p_scene);
    void UpdateMainPass(const Scene& p_scene);
    void UpdateBloomConstants();

    void FillPass(const Scene& p_scene, PassContext& p_pass, FilterObjectFunc1 p_filter1, FilterObjectFunc2 p_filter2);
};

}  // namespace my
