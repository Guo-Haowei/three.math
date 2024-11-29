#include "graphics_manager.h"

#include "engine/core/base/random.h"
#include "engine/core/debugger/profiler.h"
#include "engine/core/framework/application.h"
#include "engine/core/framework/scene_manager.h"
#include "engine/core/math/frustum.h"
#include "engine/core/math/matrix_transform.h"
#if USING(PLATFORM_WINDOWS)
#include "engine/drivers/d3d11/d3d11_graphics_manager.h"
#include "engine/drivers/d3d12/d3d12_graphics_manager.h"
#include "engine/drivers/vk/vulkan_graphics_manager.h"
#elif USING(PLATFORM_APPLE)
#include "engine/drivers/metal/metal_graphics_manager.h"
#endif
#include "engine/drivers/empty/empty_graphics_manager.h"
#include "engine/drivers/opengl/opengl_graphics_manager.h"
#include "engine/renderer/graphics_dvars.h"
#include "engine/renderer/render_graph/pass_creator.h"
#include "engine/renderer/render_graph/render_graph_defines.h"
#include "engine/renderer/render_manager.h"
#include "shader_resource_defines.hlsl.h"

// @TODO: refactor
#include "engine/renderer/path_tracer/path_tracer.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef GetMessage
#undef GetMessage
#endif

namespace my {

const char* ToString(RenderGraphName p_name) {
    ERR_FAIL_INDEX_V(p_name, RenderGraphName::COUNT, nullptr);
    static constexpr const char* s_table[] = {
#define RENDER_GRAPH_DECLARE(ENUM, STR) STR,
        RENDER_GRAPH_LIST
#undef RENDER_GRAPH_DECLARE
    };

    return s_table[std::to_underlying(p_name)];
}

template<typename T>
static auto CreateUniformCheckSize(GraphicsManager& p_graphics_manager, uint32_t p_max_count) {
    static_assert(sizeof(T) % 256 == 0);
    GpuBufferDesc buffer_desc{};
    buffer_desc.slot = T::GetUniformBufferSlot();
    buffer_desc.elementCount = p_max_count;
    buffer_desc.elementSize = sizeof(T);
    return p_graphics_manager.CreateConstantBuffer(buffer_desc);
}

ConstantBuffer<PerSceneConstantBuffer> g_constantCache;
ConstantBuffer<DebugDrawConstantBuffer> g_debug_draw_cache;
ConstantBuffer<EnvConstantBuffer> g_env_cache;

// @TODO: refactor this
template<typename T>
static void CreateUniformBuffer(ConstantBuffer<T>& p_buffer) {
    GpuBufferDesc buffer_desc{};
    buffer_desc.slot = T::GetUniformBufferSlot();
    buffer_desc.elementCount = 1;
    buffer_desc.elementSize = sizeof(T);
    p_buffer.buffer = *GraphicsManager::GetSingleton().CreateConstantBuffer(buffer_desc);
}

auto GraphicsManager::InitializeImpl() -> Result<void> {
    m_enableValidationLayer = DVAR_GET_BOOL(gfx_gpu_validation);

    const int num_frames = (GetBackend() == Backend::D3D12) ? NUM_FRAMES_IN_FLIGHT : 1;
    m_frameContexts.resize(num_frames);
    for (int i = 0; i < num_frames; ++i) {
        m_frameContexts[i] = CreateFrameContext();
    }
    if (auto res = InitializeInternal(); !res) {
        return HBN_ERROR(res.error());
    }
    if (auto res = SelectRenderGraph(); !res) {
        return HBN_ERROR(res.error());
    }

    for (int i = 0; i < num_frames; ++i) {
        FrameContext& frame_context = *m_frameContexts[i].get();
        frame_context.batchCb = *::my::CreateUniformCheckSize<PerBatchConstantBuffer>(*this, 4096 * 16);
        frame_context.passCb = *::my::CreateUniformCheckSize<PerPassConstantBuffer>(*this, 32);
        frame_context.materialCb = *::my::CreateUniformCheckSize<MaterialConstantBuffer>(*this, 2048 * 16);
        frame_context.boneCb = *::my::CreateUniformCheckSize<BoneConstantBuffer>(*this, 16);
        frame_context.emitterCb = *::my::CreateUniformCheckSize<EmitterConstantBuffer>(*this, 32);
        frame_context.pointShadowCb = *::my::CreateUniformCheckSize<PointShadowConstantBuffer>(*this, 6 * MAX_POINT_LIGHT_SHADOW_COUNT);
        frame_context.perFrameCb = *::my::CreateUniformCheckSize<PerFrameConstantBuffer>(*this, 1);
    }

    // @TODO: refactor
    CreateUniformBuffer<PerSceneConstantBuffer>(g_constantCache);
    CreateUniformBuffer<DebugDrawConstantBuffer>(g_debug_draw_cache);
    CreateUniformBuffer<EnvConstantBuffer>(g_env_cache);

    DEV_ASSERT(m_pipelineStateManager);

    if (auto res = m_pipelineStateManager->Initialize(); !res) {
        return HBN_ERROR(res.error());
    }

    auto bind_slot = [&](RenderTargetResourceName p_name, int p_slot) {
        std::shared_ptr<GpuTexture> texture = FindTexture(p_name);
        if (!texture) {
            return;
        }

        DEV_ASSERT(p_slot >= 0);
        texture->slot = p_slot;
    };
#define SRV(TYPE, NAME, SLOT, BINDING) bind_slot(BINDING, SLOT);
    SRV_DEFINES
#undef SRV

    m_initialized = true;
    return Result<void>();
}

void GraphicsManager::EventReceived(std::shared_ptr<IEvent> p_event) {
    if (SceneChangeEvent* e = dynamic_cast<SceneChangeEvent*>(p_event.get()); e) {
        OnSceneChange(*e->GetScene());
    }
    if (ResizeEvent* e = dynamic_cast<ResizeEvent*>(p_event.get()); e) {
        OnWindowResize(e->GetWidth(), e->GetHeight());
    }
}

auto GraphicsManager::Create() -> Result<std::shared_ptr<GraphicsManager>> {
    const std::string& backend = DVAR_GET_STRING(gfx_backend);

    auto select_renderer = [&]() -> std::shared_ptr<GraphicsManager> {
        auto create_empty_renderer = []() -> std::shared_ptr<GraphicsManager> {
            return std::make_shared<EmptyGraphicsManager>("EmptyGraphicsManager", Backend::EMPTY, 1);
        };

        auto create_d3d11_renderer = []() -> std::shared_ptr<GraphicsManager> {
#if USING(PLATFORM_WINDOWS)
            return std::make_shared<D3d11GraphicsManager>();
#else
            return nullptr;
#endif
        };
        auto create_d3d12_renderer = []() -> std::shared_ptr<GraphicsManager> {
#if USING(PLATFORM_WINDOWS)
            return std::make_shared<D3d12GraphicsManager>();
#else
            return nullptr;
#endif
        };
        auto create_vulkan_renderer = []() -> std::shared_ptr<GraphicsManager> {
#if USING(PLATFORM_WINDOWS)
            return std::make_shared<VulkanGraphicsManager>();
#else
            return nullptr;
#endif
        };
        auto create_opengl_renderer = []() -> std::shared_ptr<GraphicsManager> {
#if USING(PLATFORM_WINDOWS)
            return std::make_shared<OpenGlGraphicsManager>();
#else
            return nullptr;
#endif
        };
        auto create_metal_renderer = []() -> std::shared_ptr<GraphicsManager> {
#if USING(PLATFORM_APPLE)
            return std::make_shared<MetalGraphicsManager>();
#else
            return nullptr;
#endif
        };

#define BACKEND_DECLARE(ENUM, DISPLAY, DVAR) \
    if (backend == #DVAR) { return create_##DVAR##_renderer(); }
        BACKEND_LIST
#undef BACKEND_DECLARE
        return nullptr;
    };

    auto result = select_renderer();
    if (!result) {
        return HBN_ERROR(ErrorCode::ERR_CANT_CREATE, "backend '{}' not supported", backend);
    }
    return result;
}

void GraphicsManager::SetPipelineState(PipelineStateName p_name) {
    SetPipelineStateImpl(p_name);
}

void GraphicsManager::RequestTexture(ImageHandle* p_handle, OnTextureLoadFunc p_func) {
    m_loadedImages.push(ImageTask{ p_handle, p_func });
}

void GraphicsManager::Update(Scene& p_scene) {
    OPTICK_EVENT();

    [[maybe_unused]] const Backend backend = GetBackend();

    Cleanup();

    UpdateConstants(p_scene);
    UpdateForceFields(p_scene);
    UpdateLights(p_scene);
    UpdateVoxelPass(p_scene);
    UpdateMainPass(p_scene);
    UpdateBloomConstants();

    // @TODO: make it a function
    auto loaded_images = m_loadedImages.pop_all();
    while (!loaded_images.empty()) {
        auto task = loaded_images.front();
        loaded_images.pop();
        DEV_ASSERT(task.handle->state == ASSET_STATE_READY);
        Image* image = task.handle->Get();
        DEV_ASSERT(image);

        GpuTextureDesc texture_desc{};
        SamplerDesc sampler_desc{};
        renderer::fill_texture_and_sampler_desc(image, texture_desc, sampler_desc);

        image->gpu_texture = CreateTexture(texture_desc, sampler_desc);
        if (task.func) {
            task.func(task.handle->Get());
        }
    }

    {
        OPTICK_EVENT("Render");
        BeginFrame();

        UpdateEmitters(p_scene);

        auto& frame = GetCurrentFrame();
        UpdateConstantBuffer(frame.batchCb.get(), frame.batchCache.buffer);
        UpdateConstantBuffer(frame.materialCb.get(), frame.materialCache.buffer);
        UpdateConstantBuffer(frame.boneCb.get(), frame.boneCache.buffer);
        UpdateConstantBuffer(frame.passCb.get(), frame.passCache);
        UpdateConstantBuffer(frame.emitterCb.get(), frame.emitterCache);
        UpdateConstantBuffer<PointShadowConstantBuffer, 6 * MAX_POINT_LIGHT_SHADOW_COUNT>(frame.pointShadowCb.get(), frame.pointShadowCache);
        UpdateConstantBuffer(frame.perFrameCb.get(), &frame.perFrameCache, sizeof(PerFrameConstantBuffer));
        BindConstantBufferSlot<PerFrameConstantBuffer>(frame.perFrameCb.get(), 0);

        // @HACK
        if (backend != Backend::VULKAN && backend != Backend::METAL) {
            auto graph = GetActiveRenderGraph();
            if (DEV_VERIFY(graph)) {
                graph->Execute(*this);
            }
        }

        Render();
        EndFrame();
        Present();
        MoveToNextFrame();
    }
}

void GraphicsManager::UpdateBufferData(const GpuBufferDesc& p_desc, const GpuStructuredBuffer* p_buffer) {
    unused(p_desc);
    unused(p_buffer);
}

void GraphicsManager::BeginFrame() {
}

void GraphicsManager::EndFrame() {
}

void GraphicsManager::MoveToNextFrame() {
}

std::unique_ptr<FrameContext> GraphicsManager::CreateFrameContext() {
    return std::make_unique<FrameContext>();
}

void GraphicsManager::BeginDrawPass(const DrawPass* p_draw_pass) {
    for (auto& texture : p_draw_pass->outSrvs) {
        if (texture->slot >= 0) {
            UnbindTexture(Dimension::TEXTURE_2D, texture->slot);
            // RT_DEBUG("  -- unbound resource '{}'({})", RenderTargetResourceNameToString(it->desc.name), it->slot);
        }
    }

    for (auto& transition : p_draw_pass->desc.transitions) {
        if (transition.beginPassFunc) {
            transition.beginPassFunc(this, transition.resource.get(), transition.slot);
        }
    }
}

void GraphicsManager::EndDrawPass(const DrawPass* p_draw_pass) {
    UnsetRenderTarget();
    for (auto& texture : p_draw_pass->outSrvs) {
        if (texture->slot >= 0) {
            BindTexture(Dimension::TEXTURE_2D, texture->GetHandle(), texture->slot);
            // RT_DEBUG("  -- bound resource '{}'({})", RenderTargetResourceNameToString(it->desc.name), it->slot);
        }
    }

    for (auto& transition : p_draw_pass->desc.transitions) {
        if (transition.endPassFunc) {
            transition.endPassFunc(this, transition.resource.get(), transition.slot);
        }
    }
}

auto GraphicsManager::SelectRenderGraph() -> Result<void> {
    std::string method(DVAR_GET_STRING(gfx_render_graph));
    static const std::map<std::string, RenderGraphName> lookup = {
#define RENDER_GRAPH_DECLARE(ENUM, STR) \
    { STR, RenderGraphName::ENUM },
        RENDER_GRAPH_LIST
#undef RENDER_GRAPH_DECLARE
    };

    if (!method.empty()) {
        auto it = lookup.find(method);
        if (it == lookup.end()) {
            return HBN_ERROR(ErrorCode::ERR_INVALID_PARAMETER, "unknown render graph '{}'", method);
        } else {
            m_activeRenderGraphName = it->second;
        }
    }

    switch (GetBackend()) {
        case Backend::VULKAN:
        case Backend::EMPTY:
        case Backend::METAL:
            m_activeRenderGraphName = RenderGraphName::DUMMY;
            break;
        default:
            break;
    }

    // force to default
    if (m_backend != Backend::OPENGL && m_activeRenderGraphName == RenderGraphName::EXPERIMENTAL) {
        LOG_WARN("'experimental' not supported, fall back to 'default'");
        m_activeRenderGraphName = RenderGraphName::DEFAULT;
    }

    switch (m_activeRenderGraphName) {
        case RenderGraphName::DUMMY:
            m_renderGraphs[std::to_underlying(RenderGraphName::DUMMY)] = rg::RenderPassCreator::CreateDummy();
            break;
        case RenderGraphName::EXPERIMENTAL:
            m_renderGraphs[std::to_underlying(RenderGraphName::EXPERIMENTAL)] = rg::RenderPassCreator::CreateExperimental();
            break;
        case RenderGraphName::DEFAULT:
            m_renderGraphs[std::to_underlying(RenderGraphName::DEFAULT)] = rg::RenderPassCreator::CreateDefault();
            break;
        default:
            DEV_ASSERT(0 && "Should not reach here");
            return HBN_ERROR(ErrorCode::ERR_INVALID_PARAMETER, "unknown render graph '{}'", method);
    }

    switch (m_backend) {
        case Backend::OPENGL:
        case Backend::D3D11:
            m_renderGraphs[std::to_underlying(RenderGraphName::PATHTRACER)] = rg::RenderPassCreator::CreatePathTracer();
            break;
        default:
            break;
    }

    return Result<void>();
}

bool GraphicsManager::SetActiveRenderGraph(RenderGraphName p_name) {
    ERR_FAIL_INDEX_V(p_name, RenderGraphName::COUNT, false);
    const int index = std::to_underlying(p_name);
    if (!m_renderGraphs[index]) {
        return false;
    }

    if (p_name == m_activeRenderGraphName) {
        return false;
    }

    m_activeRenderGraphName = p_name;
    return true;
}

rg::RenderGraph* GraphicsManager::GetActiveRenderGraph() {
    const int index = std::to_underlying(m_activeRenderGraphName);
    ERR_FAIL_INDEX_V(index, RenderGraphName::COUNT, nullptr);
    DEV_ASSERT(m_renderGraphs[index] != nullptr);
    return m_renderGraphs[index].get();
}

bool GraphicsManager::StartPathTracer(PathTracerMethod p_method) {
    unused(p_method);
    if (m_pathTracerGeometryBuffer) {
        return true;
    }

    // @TODO: refactor
    DEV_ASSERT(m_activeRenderGraphName == RenderGraphName::PATHTRACER);
    DEV_ASSERT(m_backend == Backend::OPENGL || m_backend == Backend::D3D11);

    SceneManager* scene_manager = m_app->GetSceneManager();
    Scene* scene = scene_manager->GetScenePtr();
    if (DEV_VERIFY(scene)) {
        GpuScene gpu_scene;
        ConstructScene(*scene, gpu_scene);

        const uint32_t geometry_count = (uint32_t)gpu_scene.geometries.size();
        const uint32_t bvh_count = (uint32_t)gpu_scene.bvhs.size();
        const uint32_t material_count = (uint32_t)gpu_scene.materials.size();

        {
            GpuBufferDesc desc{
                .slot = GetGlobalGeometriesSlot(),
                .elementSize = sizeof(gpu_geometry_t),
                .elementCount = geometry_count,
                .initialData = gpu_scene.geometries.data(),
            };
            m_pathTracerGeometryBuffer = *CreateStructuredBuffer(desc);
        }
        {
            GpuBufferDesc desc{
                .slot = GetGlobalGeometriesSlot(),
                .elementSize = sizeof(gpu_bvh_t),
                .elementCount = bvh_count,
                .initialData = gpu_scene.bvhs.data(),
            };
            m_pathTracerBvhBuffer = *CreateStructuredBuffer(desc);
        }
        {
            GpuBufferDesc desc{
                .slot = GetGlobalMaterialsSlot(),
                .elementSize = sizeof(gpu_material_t),
                .elementCount = material_count,
                .initialData = gpu_scene.materials.data(),
            };
            m_pathTracerMaterialBuffer = *CreateStructuredBuffer(desc);
        }

        LOG("Path tracer scene loaded, contains {} triangles, {} BVH", geometry_count, bvh_count);

        return true;
    }

    return false;
}

std::shared_ptr<GpuTexture> GraphicsManager::CreateTexture(const GpuTextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc) {
    auto texture = CreateTextureImpl(p_texture_desc, p_sampler_desc);
    if (p_texture_desc.type != AttachmentType::NONE) {
        DEV_ASSERT(m_resourceLookup.find(p_texture_desc.name) == m_resourceLookup.end());
        m_resourceLookup[p_texture_desc.name] = texture;
    }
    return texture;
}

std::shared_ptr<GpuTexture> GraphicsManager::FindTexture(RenderTargetResourceName p_name) const {
    if (m_resourceLookup.empty()) {
        return nullptr;
    }

    auto it = m_resourceLookup.find(p_name);
    if (it == m_resourceLookup.end()) {
        return nullptr;
    }
    return it->second;
}

uint64_t GraphicsManager::GetFinalImage() const {
    const GpuTexture* texture = nullptr;
    switch (m_activeRenderGraphName) {
        case RenderGraphName::DUMMY:
            texture = FindTexture(RESOURCE_GBUFFER_BASE_COLOR).get();
            break;
        case RenderGraphName::EXPERIMENTAL:
            texture = FindTexture(RESOURCE_FINAL).get();
            break;
        case RenderGraphName::DEFAULT:
            texture = FindTexture(RESOURCE_TONE).get();
            break;
        case RenderGraphName::PATHTRACER:
            texture = FindTexture(RESOURCE_PATH_TRACER).get();
            // texture = FindTexture(RESOURCE_TONE).get();
            break;
        default:
            CRASH_NOW();
            break;
    }

    if (texture) {
        return texture->GetHandle();
    }

    return 0;
}

void GraphicsManager::Cleanup() {
    auto& frame_context = GetCurrentFrame();
    frame_context.batchCache.Clear();
    frame_context.materialCache.Clear();
    frame_context.boneCache.Clear();
    frame_context.passCache.clear();
    frame_context.emitterCache.clear();

    for (auto& pass : m_shadowPasses) {
        pass.draws.clear();
    }

    for (auto& pass : m_pointShadowPasses) {
        pass.reset();
    }

    m_mainPass.draws.clear();
    m_voxelPass.draws.clear();
}

void GraphicsManager::UpdateConstants(const Scene& p_scene) {
    Camera& camera = *p_scene.m_camera.get();

    auto& cache = GetCurrentFrame().perFrameCache;
    cache.c_cameraPosition = camera.GetPosition();

    cache.c_enableVxgi = DVAR_GET_BOOL(gfx_enable_vxgi);
    cache.c_debugVoxelId = DVAR_GET_INT(gfx_debug_vxgi_voxel);
    cache.c_noTexture = DVAR_GET_BOOL(gfx_no_texture);

    // Bloom
    cache.c_bloomThreshold = DVAR_GET_FLOAT(gfx_bloom_threshold);
    cache.c_enableBloom = DVAR_GET_BOOL(gfx_enable_bloom);

    // @TODO: refactor the following
    const int voxel_texture_size = DVAR_GET_INT(gfx_voxel_size);
    DEV_ASSERT(math::IsPowerOfTwo(voxel_texture_size));
    DEV_ASSERT(voxel_texture_size <= 256);

    Vector3f world_center = p_scene.GetBound().Center();
    Vector3f aabb_size = p_scene.GetBound().Size();
    float world_size = glm::max(aabb_size.x, glm::max(aabb_size.y, aabb_size.z));

    const float max_world_size = DVAR_GET_FLOAT(gfx_vxgi_max_world_size);
    if (world_size > max_world_size) {
        world_center = camera.GetPosition();
        world_size = max_world_size;
    }

    const float texel_size = 1.0f / static_cast<float>(voxel_texture_size);
    const float voxel_size = world_size * texel_size;

    cache.c_worldCenter = world_center;
    cache.c_worldSizeHalf = 0.5f * world_size;
    cache.c_texelSize = texel_size;
    cache.c_voxelSize = voxel_size;
    cache.c_cameraForward = camera.GetFront();
    // cache.c_tileOffset;
    cache.c_cameraRight = camera.GetRight();
    // @TODO: refactor
    static int s_frameIndex = 0;
    cache.c_cameraUp = glm::cross(cache.c_cameraForward, cache.c_cameraRight);
    cache.c_frameIndex = s_frameIndex++;
    cache.c_cameraFov = camera.GetFovy().GetDegree();
    cache.c_sceneDirty = camera.IsDirty();

    // Force fields

    int counter = 0;
    for (auto [id, force_field_component] : p_scene.m_ForceFieldComponents) {
        ForceField& force_field = cache.c_forceFields[counter++];
        const TransformComponent& transform = *p_scene.GetComponent<TransformComponent>(id);
        force_field.position = transform.GetTranslation();
        force_field.strength = force_field_component.strength;
    }

    cache.c_forceFieldsCount = counter;

    // @TODO: cache the slots
    // Texture indices
    auto find_index = [&](RenderTargetResourceName p_name) -> uint32_t {
        std::shared_ptr<GpuTexture> resource = FindTexture(p_name);
        if (!resource) {
            return 0;
        }

        return static_cast<uint32_t>(resource->GetResidentHandle());
    };

    cache.c_GbufferBaseColorMapResidentHandle.Set32(find_index(RESOURCE_GBUFFER_BASE_COLOR));
    cache.c_GbufferPositionMapResidentHandle.Set32(find_index(RESOURCE_GBUFFER_POSITION));
    cache.c_GbufferNormalMapResidentHandle.Set32(find_index(RESOURCE_GBUFFER_NORMAL));
    cache.c_GbufferMaterialMapResidentHandle.Set32(find_index(RESOURCE_GBUFFER_MATERIAL));

    cache.c_GbufferDepthResidentHandle.Set32(find_index(RESOURCE_GBUFFER_DEPTH));
    cache.c_PointShadowArrayResidentHandle.Set32(find_index(RESOURCE_POINT_SHADOW_CUBE_ARRAY));
    cache.c_ShadowMapResidentHandle.Set32(find_index(RESOURCE_SHADOW_MAP));

    cache.c_TextureHighlightSelectResidentHandle.Set32(find_index(RESOURCE_HIGHLIGHT_SELECT));
    cache.c_TextureLightingResidentHandle.Set32(find_index(RESOURCE_LIGHTING));
}

void GraphicsManager::UpdateEmitters(const Scene& p_scene) {
    for (auto [id, emitter] : p_scene.m_ParticleEmitterComponents) {
        if (!emitter.particleBuffer) {
            // create buffer
            emitter.counterBuffer = *CreateStructuredBuffer({
                .elementSize = sizeof(ParticleCounter),
                .elementCount = 1,
            });
            emitter.deadBuffer = *CreateStructuredBuffer({
                .elementSize = sizeof(int),
                .elementCount = MAX_PARTICLE_COUNT,
            });
            emitter.aliveBuffer[0] = *CreateStructuredBuffer({
                .elementSize = sizeof(int),
                .elementCount = MAX_PARTICLE_COUNT,
            });
            emitter.aliveBuffer[1] = *CreateStructuredBuffer({
                .elementSize = sizeof(int),
                .elementCount = MAX_PARTICLE_COUNT,
            });
            emitter.particleBuffer = *CreateStructuredBuffer({
                .elementSize = sizeof(Particle),
                .elementCount = MAX_PARTICLE_COUNT,
            });

            SetPipelineState(PSO_PARTICLE_INIT);

            BindStructuredBuffer(GetGlobalParticleCounterSlot(), emitter.counterBuffer.get());
            BindStructuredBuffer(GetGlobalDeadIndicesSlot(), emitter.deadBuffer.get());
            Dispatch(MAX_PARTICLE_COUNT / PARTICLE_LOCAL_SIZE, 1, 1);
            UnbindStructuredBuffer(GetGlobalParticleCounterSlot());
            UnbindStructuredBuffer(GetGlobalDeadIndicesSlot());
        }

        const uint32_t pre_sim_idx = emitter.GetPreIndex();
        const uint32_t post_sim_idx = emitter.GetPostIndex();
        EmitterConstantBuffer buffer;
        const TransformComponent* transform = p_scene.GetComponent<TransformComponent>(id);
        buffer.c_preSimIdx = pre_sim_idx;
        buffer.c_postSimIdx = post_sim_idx;
        buffer.c_elapsedTime = p_scene.m_elapsedTime;
        buffer.c_lifeSpan = emitter.particleLifeSpan;
        buffer.c_seeds = Vector3f(Random::Float(), Random::Float(), Random::Float());
        buffer.c_emitterScale = emitter.particleScale;
        buffer.c_emitterPosition = transform->GetTranslation();
        buffer.c_particlesPerFrame = emitter.particlesPerFrame;
        buffer.c_emitterStartingVelocity = emitter.startingVelocity;
        buffer.c_emitterMaxParticleCount = emitter.maxParticleCount;
        buffer.c_emitterHasGravity = emitter.gravity;

        GetCurrentFrame().emitterCache.push_back(buffer);
    }
}

void GraphicsManager::UpdateForceFields(const Scene& p_scene) {
    unused(p_scene);
}

/// @TODO: refactor lights
void GraphicsManager::UpdateLights(const Scene& p_scene) {
    const uint32_t light_count = glm::min<uint32_t>((uint32_t)p_scene.GetCount<LightComponent>(), MAX_LIGHT_COUNT);

    auto& cache = GetCurrentFrame().perFrameCache;
    cache.c_lightCount = light_count;

    auto& point_shadow_cache = GetCurrentFrame().pointShadowCache;

    int idx = 0;
    for (auto [light_entity, light_component] : p_scene.m_LightComponents) {
        const TransformComponent* light_transform = p_scene.GetComponent<TransformComponent>(light_entity);
        const MaterialComponent* material = p_scene.GetComponent<MaterialComponent>(light_entity);

        DEV_ASSERT(light_transform && material);

        // SHOULD BE THIS INDEX
        Light& light = cache.c_lights[idx];
        bool cast_shadow = light_component.CastShadow();
        light.cast_shadow = cast_shadow;
        light.type = light_component.GetType();
        light.color = material->baseColor;
        light.color *= material->emissive;
        switch (light_component.GetType()) {
            case LIGHT_TYPE_INFINITE: {
                Matrix4x4f light_local_matrix = light_transform->GetLocalMatrix();
                Vector3f light_dir = glm::normalize(light_local_matrix * Vector4f(0, 0, 1, 0));
                light.cast_shadow = cast_shadow;
                light.position = light_dir;

                const AABB& world_bound = p_scene.GetBound();
                const Vector3f center = world_bound.Center();
                const Vector3f extents = world_bound.Size();
                const float size = 0.7f * glm::max(extents.x, glm::max(extents.y, extents.z));

                light.view_matrix = glm::lookAt(center + light_dir * size, center, Vector3f(0, 1, 0));

                if (GetBackend() == Backend::OPENGL) {
                    light.projection_matrix = BuildOpenGlOrthoRH(-size, size, -size, size, -size, 3.0f * size);
                } else {
                    light.projection_matrix = BuildOrthoRH(-size, size, -size, size, -size, 3.0f * size);
                }

                PerPassConstantBuffer pass_constant;
                // @TODO: Build correct matrices
                pass_constant.c_projectionMatrix = light.projection_matrix;
                pass_constant.c_viewMatrix = light.view_matrix;
                m_shadowPasses[0].pass_idx = static_cast<int>(GetCurrentFrame().passCache.size());
                GetCurrentFrame().passCache.emplace_back(pass_constant);

                Frustum light_frustum(light.projection_matrix * light.view_matrix);
                FillPass(
                    p_scene,
                    m_shadowPasses[0],
                    [](const ObjectComponent& p_object) {
                        return p_object.flags & ObjectComponent::CAST_SHADOW;
                    },
                    [&](const AABB& p_aabb) {
                        return light_frustum.Intersects(p_aabb);
                    });
            } break;
            case LIGHT_TYPE_POINT: {
                const int shadow_map_index = light_component.GetShadowMapIndex();
                // @TODO: there's a bug in shadow map allocation
                light.atten_constant = light_component.m_atten.constant;
                light.atten_linear = light_component.m_atten.linear;
                light.atten_quadratic = light_component.m_atten.quadratic;
                light.position = light_component.GetPosition();
                light.cast_shadow = cast_shadow;
                light.max_distance = light_component.GetMaxDistance();
                if (cast_shadow && shadow_map_index != INVALID_POINT_SHADOW_HANDLE) {
                    light.shadow_map_index = shadow_map_index;

                    Vector3f radiance(light.max_distance);
                    AABB aabb = AABB::FromCenterSize(light.position, radiance);
                    auto pass = std::make_unique<PassContext>();
                    FillPass(
                        p_scene,
                        *pass.get(),
                        [](const ObjectComponent& p_object) {
                            return p_object.flags & ObjectComponent::CAST_SHADOW;
                        },
                        [&](const AABB& p_aabb) {
                            return p_aabb.Intersects(aabb);
                        });

                    DEV_ASSERT_INDEX(shadow_map_index, MAX_POINT_LIGHT_SHADOW_COUNT);
                    const auto& light_matrices = light_component.GetMatrices();
                    for (int face_id = 0; face_id < 6; ++face_id) {
                        const uint32_t slot = shadow_map_index * 6 + face_id;
                        point_shadow_cache[slot].c_pointLightMatrix = light_matrices[face_id];
                        point_shadow_cache[slot].c_pointLightPosition = light_component.GetPosition();
                        point_shadow_cache[slot].c_pointLightFar = light_component.GetMaxDistance();
                    }

                    m_pointShadowPasses[shadow_map_index] = std::move(pass);
                } else {
                    light.shadow_map_index = -1;
                }
            } break;
            case LIGHT_TYPE_AREA: {
                Matrix4x4f transform = light_transform->GetWorldMatrix();
                constexpr float s = 0.5f;
                light.points[0] = transform * Vector4f(-s, +s, 0.0f, 1.0f);
                light.points[1] = transform * Vector4f(-s, -s, 0.0f, 1.0f);
                light.points[2] = transform * Vector4f(+s, -s, 0.0f, 1.0f);
                light.points[3] = transform * Vector4f(+s, +s, 0.0f, 1.0f);
            } break;
            default:
                CRASH_NOW();
                break;
        }
        ++idx;
    }
}

void GraphicsManager::UpdateVoxelPass(const Scene& p_scene) {
    if (!DVAR_GET_BOOL(gfx_enable_vxgi)) {
        return;
    }
    FillPass(
        p_scene,
        m_voxelPass,
        [](const ObjectComponent& object) {
            return object.flags & ObjectComponent::RENDERABLE;
        },
        [&](const AABB& aabb) {
            unused(aabb);
            // return scene->get_bound().intersects(aabb);
            return true;
        });
}

void GraphicsManager::UpdateMainPass(const Scene& p_scene) {
    const Camera& camera = *p_scene.m_camera;
    Frustum camera_frustum(camera.GetProjectionViewMatrix());

    // main pass
    PerPassConstantBuffer pass_constant;
    pass_constant.c_viewMatrix = camera.GetViewMatrix();

    const float fovy = camera.GetFovy().ToRad();
    const float aspect = camera.GetAspect();
    if (GetBackend() == Backend::OPENGL) {
        pass_constant.c_projectionMatrix = BuildOpenGlPerspectiveRH(fovy, aspect, camera.GetNear(), camera.GetFar());
    } else {
        pass_constant.c_projectionMatrix = BuildPerspectiveRH(fovy, aspect, camera.GetNear(), camera.GetFar());
    }

    m_mainPass.pass_idx = static_cast<int>(GetCurrentFrame().passCache.size());
    GetCurrentFrame().passCache.emplace_back(pass_constant);

    FillPass(
        p_scene,
        m_mainPass,
        [](const ObjectComponent& object) {
            return object.flags & ObjectComponent::RENDERABLE;
        },
        [&](const AABB& aabb) {
            return camera_frustum.Intersects(aabb);
        });
}

void GraphicsManager::UpdateBloomConstants() {
    auto image = FindTexture(RESOURCE_BLOOM_0).get();
    if (!image) {
        return;
    }
    constexpr int count = BLOOM_MIP_CHAIN_MAX * 2 - 1;
    auto& frame = GetCurrentFrame();
    if (frame.batchCache.buffer.size() < count) {
        frame.batchCache.buffer.resize(count);
    }

    int offset = 0;
    frame.batchCache.buffer[offset++].c_BloomOutputImageResidentHandle.Set32((uint)image->GetUavHandle());

    for (int i = 0; i < BLOOM_MIP_CHAIN_MAX - 1; ++i) {
        auto input = FindTexture(static_cast<RenderTargetResourceName>(RESOURCE_BLOOM_0 + i));
        auto output = FindTexture(static_cast<RenderTargetResourceName>(RESOURCE_BLOOM_0 + i + 1));

        frame.batchCache.buffer[i + offset].c_BloomInputTextureResidentHandle.Set32((uint)input->GetResidentHandle());
        frame.batchCache.buffer[i + offset].c_BloomOutputImageResidentHandle.Set32((uint)output->GetUavHandle());
    }

    offset += BLOOM_MIP_CHAIN_MAX - 1;

    for (int i = BLOOM_MIP_CHAIN_MAX - 1; i > 0; --i) {
        auto input = FindTexture(static_cast<RenderTargetResourceName>(RESOURCE_BLOOM_0 + i));
        auto output = FindTexture(static_cast<RenderTargetResourceName>(RESOURCE_BLOOM_0 + i - 1));

        frame.batchCache.buffer[i - 1 + offset].c_BloomInputTextureResidentHandle.Set32((uint)input->GetResidentHandle());
        frame.batchCache.buffer[i - 1 + offset].c_BloomOutputImageResidentHandle.Set32((uint)output->GetUavHandle());
    }
}

void GraphicsManager::FillPass(const Scene& p_scene, PassContext& p_pass, FilterObjectFunc1 p_filter1, FilterObjectFunc2 p_filter2) {
    const bool is_opengl = m_backend == Backend::OPENGL;
    auto FillMaterialConstantBuffer = [&](const MaterialComponent* material, MaterialConstantBuffer& cb) {
        cb.c_baseColor = material->baseColor;
        cb.c_metallic = material->metallic;
        cb.c_roughness = material->roughness;
        cb.c_emissivePower = material->emissive;

        auto set_texture = [&](int p_idx,
                               TextureHandle& p_out_handle,
                               sampler_t& p_out_resident_handle) {
            p_out_handle = 0;
            p_out_resident_handle.Set64(0);

            if (!material->textures[p_idx].enabled) {
                return false;
            }

            ImageHandle* handle = material->textures[p_idx].image;
            if (!handle) {
                return false;
            }

            Image* image = handle->Get();
            if (!image) {
                return false;
            }

            auto texture = reinterpret_cast<GpuTexture*>(image->gpu_texture.get());
            if (!texture) {
                return false;
            }

            const uint64_t resident_handle = texture->GetResidentHandle();

            p_out_handle = texture->GetHandle();
            if (is_opengl) {
                p_out_resident_handle.Set64(resident_handle);
            } else {
                p_out_resident_handle.Set32(static_cast<uint32_t>(resident_handle));
            }
            return true;
        };

        cb.c_hasBaseColorMap = set_texture(MaterialComponent::TEXTURE_BASE, cb.c_baseColorMapHandle, cb.c_BaseColorMapResidentHandle);
        cb.c_hasNormalMap = set_texture(MaterialComponent::TEXTURE_NORMAL, cb.c_normalMapHandle, cb.c_NormalMapResidentHandle);
        cb.c_hasMaterialMap = set_texture(MaterialComponent::TEXTURE_METALLIC_ROUGHNESS, cb.c_materialMapHandle, cb.c_MaterialMapResidentHandle);
    };

    for (auto [entity, obj] : p_scene.m_ObjectComponents) {
        if (!p_scene.Contains<TransformComponent>(entity)) {
            continue;
        }

        if (!p_filter1(obj)) {
            continue;
        }

        const TransformComponent& transform = *p_scene.GetComponent<TransformComponent>(entity);
        DEV_ASSERT(p_scene.Contains<MeshComponent>(obj.meshId));
        const MeshComponent& mesh = *p_scene.GetComponent<MeshComponent>(obj.meshId);

        const Matrix4x4f& world_matrix = transform.GetWorldMatrix();
        AABB aabb = mesh.localBound;
        aabb.ApplyMatrix(world_matrix);
        if (!p_filter2(aabb)) {
            continue;
        }

        PerBatchConstantBuffer batch_buffer;
        batch_buffer.c_worldMatrix = world_matrix;
        batch_buffer.c_hasAnimation = mesh.armatureId.IsValid();

        BatchContext draw;
        draw.flags = 0;
        if (entity == p_scene.m_selected) {
            draw.flags |= STENCIL_FLAG_SELECTED;
        }

        draw.batch_idx = GetCurrentFrame().batchCache.FindOrAdd(entity, batch_buffer);
        if (mesh.armatureId.IsValid()) {
            auto& armature = *p_scene.GetComponent<ArmatureComponent>(mesh.armatureId);
            DEV_ASSERT(armature.boneTransforms.size() <= MAX_BONE_COUNT);

            BoneConstantBuffer bone;
            memcpy(bone.c_bones, armature.boneTransforms.data(), sizeof(Matrix4x4f) * armature.boneTransforms.size());

            // @TODO: better memory usage
            draw.bone_idx = GetCurrentFrame().boneCache.FindOrAdd(mesh.armatureId, bone);
        } else {
            draw.bone_idx = -1;
        }
        if (!mesh.gpuResource) {
            continue;
        }

        draw.mesh_data = (MeshBuffers*)mesh.gpuResource;

        for (const auto& subset : mesh.subsets) {
            aabb = subset.local_bound;
            aabb.ApplyMatrix(world_matrix);
            if (!p_filter2(aabb)) {
                continue;
            }

            const MaterialComponent* material = p_scene.GetComponent<MaterialComponent>(subset.material_id);
            MaterialConstantBuffer material_buffer;
            FillMaterialConstantBuffer(material, material_buffer);

            DrawContext sub_mesh;
            sub_mesh.index_count = subset.index_count;
            sub_mesh.index_offset = subset.index_offset;
            sub_mesh.material_idx = GetCurrentFrame().materialCache.FindOrAdd(subset.material_id, material_buffer);

            draw.subsets.emplace_back(std::move(sub_mesh));
        }

        p_pass.draws.emplace_back(std::move(draw));
    }
}

}  // namespace my
