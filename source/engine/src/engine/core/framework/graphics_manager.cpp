#include "graphics_manager.h"

#include "core/debugger/profiler.h"
#include "core/math/frustum.h"
#include "drivers/d3d11/d3d11_graphics_manager.h"
#include "drivers/empty/empty_graphics_manager.h"
#include "drivers/opengl/opengl_graphics_manager.h"
#include "rendering/render_graph/render_graph_defines.h"
#include "rendering/render_manager.h"
#include "rendering/rendering_dvars.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace my {

// @TODO: refactor
static mat4 buildPerspectiveRH(float p_fovy, float p_aspect, float p_near, float p_far) {
    const float tan_half_fovy = glm::tan(0.5f * p_fovy);
    mat4 Result(0.0f);
    Result[0][0] = 1.0f / (p_aspect * tan_half_fovy);
    Result[1][1] = 1.0f / tan_half_fovy;
    Result[2][2] = -p_far / (p_far - p_near);
    Result[2][3] = -1.0f;
    Result[3][2] = -(p_far * p_near) / (p_far - p_near);
    return Result;
}

static mat4 buildOpenGLPerspectiveRH(float p_fovy, float p_aspect, float p_near, float p_far) {
    const float tan_half_fovy = glm::tan(0.5f * p_fovy);
    mat4 Result(0.0f);
    Result[0][0] = 1.0f / (p_aspect * tan_half_fovy);
    Result[1][1] = 1.0f / tan_half_fovy;
    Result[2][2] = -(p_far + p_near) / (p_far - p_near);
    Result[2][3] = -1.0f;
    Result[3][2] = -(2.0f * p_far * p_near) / (p_far - p_near);
    return Result;
}

static mat4 buildOrthoRHMatrix(const float p_left,
                               const float p_right,
                               const float p_bottom,
                               const float p_top,
                               const float p_near,
                               const float p_far) {

    const float reciprocal_width = 1.0f / (p_right - p_left);
    const float reciprocal_height = 1.0f / (p_top - p_bottom);
    const float reciprocal_depth = 1.0f / (p_far - p_near);

    mat4 result(1.0f);
    result[0][0] = 2.0f * reciprocal_width;
    result[1][1] = 2.0f * reciprocal_height;
    result[2][2] = -1.0f * reciprocal_depth;
    result[3][0] = -(p_right + p_left) * reciprocal_width;
    result[3][1] = -(p_top + p_bottom) * reciprocal_height;
    result[3][2] = -p_near * reciprocal_depth;
    return result;
}

static mat4 buildOpenGLOrthoRHMatrix(const float p_left,
                                     const float p_right,
                                     const float p_bottom,
                                     const float p_top,
                                     const float p_near,
                                     const float p_far) {

    const float reciprocal_width = 1.0f / (p_right - p_left);
    const float reciprocal_height = 1.0f / (p_top - p_bottom);
    const float reciprocal_depth = 1.0f / (p_far - p_near);

    mat4 result(1.0f);
    result[0][0] = 2.0f * reciprocal_width;
    result[1][1] = 2.0f * reciprocal_height;
    result[2][2] = -2.0f * reciprocal_depth;
    result[3][0] = -(p_right + p_left) * reciprocal_width;
    result[3][1] = -(p_top + p_bottom) * reciprocal_height;
    result[3][2] = -(p_far + p_near) * reciprocal_depth;
    return result;
}

template<typename T>
static auto create_uniform(GraphicsManager& p_graphics_manager, uint32_t p_max_count) {
    static_assert(sizeof(T) % 256 == 0);
    return p_graphics_manager.createUniform(T::get_uniform_buffer_slot(), sizeof(T) * p_max_count);
}

UniformBuffer<PerFrameConstantBuffer> g_per_frame_cache;
UniformBuffer<PerSceneConstantBuffer> g_constantCache;
UniformBuffer<DebugDrawConstantBuffer> g_debug_draw_cache;
UniformBuffer<BloomConstantBuffer> g_bloom_cache;
UniformBuffer<PointShadowConstantBuffer> g_point_shadow_cache;
UniformBuffer<EnvConstantBuffer> g_env_cache;

template<typename T>
static void create_uniform_buffer(UniformBuffer<T>& p_buffer) {
    constexpr int slot = T::get_uniform_buffer_slot();
    p_buffer.buffer = GraphicsManager::singleton().createUniform(slot, sizeof(T));
}

bool GraphicsManager::initialize() {
    if (!initializeImpl()) {
        return false;
    }

    // @TODO: refactor
    m_context.batch_uniform = create_uniform<PerBatchConstantBuffer>(*this, 4096 * 16);
    m_context.pass_uniform = create_uniform<PerPassConstantBuffer>(*this, 32);
    m_context.material_uniform = create_uniform<MaterialConstantBuffer>(*this, 2048 * 16);
    m_context.bone_uniform = create_uniform<BoneConstantBuffer>(*this, 16);

    create_uniform_buffer<PerFrameConstantBuffer>(g_per_frame_cache);
    create_uniform_buffer<PerSceneConstantBuffer>(g_constantCache);
    create_uniform_buffer<DebugDrawConstantBuffer>(g_debug_draw_cache);
    create_uniform_buffer<BloomConstantBuffer>(g_bloom_cache);
    create_uniform_buffer<PointShadowConstantBuffer>(g_point_shadow_cache);
    create_uniform_buffer<EnvConstantBuffer>(g_env_cache);

    DEV_ASSERT(m_pipeline_state_manager);

    if (!m_pipeline_state_manager->initialize()) {
        return false;
    }

    auto bind_slot = [&](RenderTargetResourceName p_name, int p_slot, Dimension p_dimension = Dimension::TEXTURE_2D) {
        std::shared_ptr<RenderTarget> resource = findRenderTarget(p_name);
        if (!resource) {
            return;
        }

        bindTexture(p_dimension, resource->texture->get_handle(), p_slot);
    };

    // bind common textures
    bind_slot(RESOURCE_HIGHLIGHT_SELECT, u_selection_highlight_slot);
    bind_slot(RESOURCE_GBUFFER_BASE_COLOR, u_gbuffer_base_color_map_slot);
    bind_slot(RESOURCE_GBUFFER_POSITION, u_gbuffer_position_map_slot);
    bind_slot(RESOURCE_GBUFFER_NORMAL, u_gbuffer_normal_map_slot);
    bind_slot(RESOURCE_GBUFFER_MATERIAL, u_gbuffer_material_map_slot);

    return true;
}

void GraphicsManager::eventReceived(std::shared_ptr<Event> event) {
    if (SceneChangeEvent* e = dynamic_cast<SceneChangeEvent*>(event.get()); e) {
        onSceneChange(*e->getScene());
    }
    if (ResizeEvent* e = dynamic_cast<ResizeEvent*>(event.get()); e) {
        onWindowResize(e->getWidth(), e->getHeight());
    }
}

std::shared_ptr<GraphicsManager> GraphicsManager::create() {
    const std::string& backend = DVAR_GET_STRING(r_backend);

    if (backend == "opengl") {
        return std::make_shared<OpenGLGraphicsManager>();
    } else if (backend == "d3d11") {
        return std::make_shared<D3d11GraphicsManager>();
    }
    return std::make_shared<EmptyGraphicsManager>(Backend::EMPTY);
}

void GraphicsManager::setPipelineState(PipelineStateName p_name) {
    if (m_last_pipeline_name != p_name) {
        setPipelineStateImpl(p_name);
        m_last_pipeline_name = p_name;
    }
}

void GraphicsManager::requestTexture(ImageHandle* p_handle, OnTextureLoadFunc p_func) {
    m_loaded_images.push(ImageTask{ p_handle, p_func });
}

void GraphicsManager::update(Scene& p_scene) {
    OPTICK_EVENT();

    cleanup();

    updateConstants(p_scene);
    updateLights(p_scene);
    updateVoxelPass(p_scene);
    updateMainPass(p_scene);

    // update uniform
    updateUniform(m_context.batch_uniform.get(), m_context.batch_cache.buffer);
    updateUniform(m_context.pass_uniform.get(), m_context.pass_cache);
    updateUniform(m_context.material_uniform.get(), m_context.material_cache.buffer);
    updateUniform(m_context.bone_uniform.get(), m_context.bone_cache.buffer);

    g_per_frame_cache.update();
    // update uniform

    // @TODO: make it a function
    auto loaded_images = m_loaded_images.pop_all();
    while (!loaded_images.empty()) {
        auto task = loaded_images.front();
        loaded_images.pop();
        DEV_ASSERT(task.handle->state == ASSET_STATE_READY);
        Image* image = task.handle->get();
        DEV_ASSERT(image);

        TextureDesc texture_desc{};
        SamplerDesc sampler_desc{};
        renderer::fill_texture_and_sampler_desc(image, texture_desc, sampler_desc);

        image->gpu_texture = createTexture(texture_desc, sampler_desc);
        if (task.func) {
            task.func(task.handle->get());
        }
    }

    render();
}

void GraphicsManager::selectRenderGraph() {
    std::string method(DVAR_GET_STRING(r_render_graph));
    if (method == "vxgi") {
        m_method = RenderGraph::VXGI;
    } else {
        m_method = RenderGraph::DEFAULT;
    }

    if (m_backend == Backend::D3D11) {
        m_method = RenderGraph::DEFAULT;
    }

    switch (m_method) {
        case RenderGraph::VXGI:
            createRenderGraphVxgi(m_render_graph);
            break;
        default:
            createRenderGraphDefault(m_render_graph);
            break;
    }
}

std::shared_ptr<RenderTarget> GraphicsManager::createRenderTarget(const RenderTargetDesc& p_desc, const SamplerDesc& p_sampler) {
    DEV_ASSERT(m_resource_lookup.find(p_desc.name) == m_resource_lookup.end());
    std::shared_ptr<RenderTarget> resource = std::make_shared<RenderTarget>(p_desc);

    // @TODO: this part need rework
    TextureDesc texture_desc{};
    switch (p_desc.type) {
        case AttachmentType::COLOR_2D:
        case AttachmentType::DEPTH_2D:
        case AttachmentType::DEPTH_STENCIL_2D:
        case AttachmentType::SHADOW_2D:
            texture_desc.dimension = Dimension::TEXTURE_2D;
            break;
        case AttachmentType::SHADOW_CUBE_MAP:
        case AttachmentType::COLOR_CUBE_MAP:
            texture_desc.dimension = Dimension::TEXTURE_CUBE;
            break;
        default:
            CRASH_NOW();
            break;
    }
    switch (p_desc.type) {
        case AttachmentType::COLOR_2D:
        case AttachmentType::COLOR_CUBE_MAP:
            texture_desc.bind_flags |= BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
            break;
        case AttachmentType::SHADOW_2D:
        case AttachmentType::SHADOW_CUBE_MAP:
            texture_desc.bind_flags |= BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
            break;
        case AttachmentType::DEPTH_2D:
        case AttachmentType::DEPTH_STENCIL_2D:
            texture_desc.bind_flags |= BIND_DEPTH_STENCIL;
            break;
        default:
            break;
    }

    texture_desc.format = p_desc.format;
    texture_desc.width = p_desc.width;
    texture_desc.height = p_desc.height;
    texture_desc.initial_data = nullptr;
    texture_desc.misc_flags = 0;
    texture_desc.mip_levels = 1;
    texture_desc.array_size = 1;
    if (p_desc.gen_mipmap) {
        texture_desc.misc_flags |= RESOURCE_MISC_GENERATE_MIPS;
    }

    resource->texture = createTexture(texture_desc, p_sampler);

    m_resource_lookup[resource->desc.name] = resource;
    return resource;
}

std::shared_ptr<RenderTarget> GraphicsManager::findRenderTarget(RenderTargetResourceName p_name) const {
    if (m_resource_lookup.empty()) {
        return nullptr;
    }

    auto it = m_resource_lookup.find(p_name);
    if (it == m_resource_lookup.end()) {
        return nullptr;
    }
    return it->second;
}

uint64_t GraphicsManager::getFinalImage() const {
    const Texture* texture = nullptr;
    switch (m_method) {
        case RenderGraph::VXGI:
            texture = findRenderTarget(RESOURCE_FINAL)->texture.get();
            break;
        case RenderGraph::DEFAULT:
            texture = findRenderTarget(RESOURCE_LIGHTING)->texture.get();
            break;
        default:
            CRASH_NOW();
            break;
    }

    if (texture) {
        return texture->get_handle();
    }

    return 0;
}

static void fill_material_constant_buffer(const MaterialComponent* material, MaterialConstantBuffer& cb) {
    cb.u_base_color = material->base_color;
    cb.u_metallic = material->metallic;
    cb.u_roughness = material->roughness;
    cb.u_emissive_power = material->emissive;

    auto set_texture = [&](int p_idx, sampler2D& p_out_handle) {
        p_out_handle = 0;

        if (!material->textures[p_idx].enabled) {
            return false;
        }

        ImageHandle* handle = material->textures[p_idx].image;
        if (!handle) {
            return false;
        }

        Image* image = handle->get();
        if (!image) {
            return false;
        }

        auto texture = reinterpret_cast<Texture*>(image->gpu_texture.get());
        if (!texture) {
            return false;
        }

        p_out_handle = texture->get_handle();
        return true;
    };

    cb.u_has_base_color_map = set_texture(MaterialComponent::TEXTURE_BASE, cb.u_base_color_map_handle);
    cb.u_has_normal_map = set_texture(MaterialComponent::TEXTURE_NORMAL, cb.u_normal_map_handle);
    cb.u_has_pbr_map = set_texture(MaterialComponent::TEXTURE_METALLIC_ROUGHNESS, cb.u_material_map_handle);
}

void GraphicsManager::cleanup() {
    m_context.batch_cache.clear();
    m_context.material_cache.clear();
    m_context.bone_cache.clear();
    m_context.pass_cache.clear();

    for (auto& pass : shadow_passes) {
        pass.draws.clear();
    }

    for (auto& pass : point_shadow_passes) {
        pass.reset();
    }

    main_pass.draws.clear();
    voxel_pass.draws.clear();
}

void GraphicsManager::updateConstants(const Scene& p_scene) {
    Camera& camera = *p_scene.m_camera.get();

    auto& cache = g_per_frame_cache.cache;
    cache.u_camera_position = camera.getPosition();

    cache.u_enable_vxgi = DVAR_GET_BOOL(r_enable_vxgi);
    cache.u_debug_voxel_id = DVAR_GET_INT(r_debug_vxgi_voxel);
    cache.u_no_texture = DVAR_GET_BOOL(r_no_texture);

    cache.u_screen_width = (int)camera.getWidth();
    cache.u_screen_height = (int)camera.getHeight();

    // Bloom
    cache.u_bloom_threshold = DVAR_GET_FLOAT(r_bloom_threshold);
    cache.u_enable_bloom = DVAR_GET_BOOL(r_enable_bloom);

    // @TODO: refactor the following
    const int voxel_texture_size = DVAR_GET_INT(r_voxel_size);
    DEV_ASSERT(math::isPowerOfTwo(voxel_texture_size));
    DEV_ASSERT(voxel_texture_size <= 256);

    vec3 world_center = p_scene.getBound().center();
    vec3 aabb_size = p_scene.getBound().size();
    float world_size = glm::max(aabb_size.x, glm::max(aabb_size.y, aabb_size.z));

    const float max_world_size = DVAR_GET_FLOAT(r_vxgi_max_world_size);
    if (world_size > max_world_size) {
        world_center = camera.getPosition();
        world_size = max_world_size;
    }

    const float texel_size = 1.0f / static_cast<float>(voxel_texture_size);
    const float voxel_size = world_size * texel_size;

    cache.u_world_center = world_center;
    cache.u_world_size_half = 0.5f * world_size;
    cache.u_texel_size = texel_size;
    cache.u_voxel_size = voxel_size;
}

/// @TODO: refactor lights
void GraphicsManager::updateLights(const Scene& p_scene) {
    const uint32_t light_count = glm::min<uint32_t>((uint32_t)p_scene.getCount<LightComponent>(), MAX_LIGHT_COUNT);

    auto& cache = g_per_frame_cache.cache;

    cache.u_light_count = light_count;

    int idx = 0;
    for (auto [light_entity, light_component] : p_scene.m_LightComponents) {
        const TransformComponent* light_transform = p_scene.getComponent<TransformComponent>(light_entity);
        const MaterialComponent* material = p_scene.getComponent<MaterialComponent>(light_entity);

        DEV_ASSERT(light_transform && material);

        // SHOULD BE THIS INDEX
        Light& light = cache.u_lights[idx];
        bool cast_shadow = light_component.castShadow();
        light.cast_shadow = cast_shadow;
        light.type = light_component.getType();
        light.color = material->base_color;
        light.color *= material->emissive;
        switch (light_component.getType()) {
            case LIGHT_TYPE_OMNI: {
                mat4 light_local_matrix = light_transform->getLocalMatrix();
                vec3 light_dir = glm::normalize(light_local_matrix * vec4(0, 0, 1, 0));
                light.cast_shadow = cast_shadow;
                light.position = light_dir;

                const AABB& world_bound = p_scene.getBound();
                const vec3 center = world_bound.center();
                const vec3 extents = world_bound.size();
                const float size = 0.7f * glm::max(extents.x, glm::max(extents.y, extents.z));

                vec3 light_up = glm::normalize(light_local_matrix * vec4(0, -1, 0, 0));

                light.view_matrix = glm::lookAt(center + light_dir * size, center, vec3(0, 1, 0));

                if (getBackend() == Backend::OPENGL) {
                    light.projection_matrix = buildOpenGLOrthoRHMatrix(-size, size, -size, size, -size, 3.0f * size);
                } else {
                    light.projection_matrix = buildOrthoRHMatrix(-size, size, -size, size, -size, 3.0f * size);
                }

                mat4 light_space_matrix = light.projection_matrix * light.view_matrix;

                PerPassConstantBuffer pass_constant;
                // @TODO: Build correct matrices
                pass_constant.g_projection_matrix = light.projection_matrix;
                pass_constant.g_view_matrix = light.view_matrix;
                shadow_passes[0].pass_idx = static_cast<int>(m_context.pass_cache.size());
                m_context.pass_cache.emplace_back(pass_constant);

                // Frustum light_frustum(light_space_matrix);
                fillPass(
                    p_scene,
                    shadow_passes[0],
                    [](const ObjectComponent& p_object) {
                        return p_object.flags & ObjectComponent::CAST_SHADOW;
                    },
                    [&](const AABB& p_aabb) {
                        unused(p_aabb);
                        // return light_frustum.intersects(aabb);
                        return true;
                    });
            } break;
            case LIGHT_TYPE_POINT: {
                const int shadow_map_index = light_component.getShadowMapIndex();
                // @TODO: there's a bug in shadow map allocation
                light.atten_constant = light_component.m_atten.constant;
                light.atten_linear = light_component.m_atten.linear;
                light.atten_quadratic = light_component.m_atten.quadratic;
                light.position = light_component.getPosition();
                light.cast_shadow = cast_shadow;
                light.max_distance = light_component.getMaxDistance();
                if (cast_shadow && shadow_map_index != INVALID_POINT_SHADOW_HANDLE) {
                    auto resource = GraphicsManager::singleton().findRenderTarget(static_cast<RenderTargetResourceName>(RESOURCE_POINT_SHADOW_MAP_0 + shadow_map_index));
                    light.shadow_map = resource ? resource->texture->get_resident_handle() : 0;

                    auto pass = std::make_unique<PassContext>();
                    fillPass(
                        p_scene,
                        *pass.get(),
                        [](const ObjectComponent& object) {
                            return object.flags & ObjectComponent::CAST_SHADOW;
                        },
                        [&](const AABB& aabb) {
                            unused(aabb);
                            return true;
                        });

                    DEV_ASSERT_INDEX(shadow_map_index, MAX_LIGHT_CAST_SHADOW_COUNT);
                    pass->light_component = light_component;

                    point_shadow_passes[shadow_map_index] = std::move(pass);
                } else {
                    light.shadow_map = 0;
                }
            } break;
            case LIGHT_TYPE_AREA: {
                mat4 transform = light_transform->getWorldMatrix();
                constexpr float s = 0.5f;
                light.points[0] = transform * vec4(-s, +s, 0.0f, 1.0f);
                light.points[1] = transform * vec4(-s, -s, 0.0f, 1.0f);
                light.points[2] = transform * vec4(+s, -s, 0.0f, 1.0f);
                light.points[3] = transform * vec4(+s, +s, 0.0f, 1.0f);
            } break;
            default:
                CRASH_NOW();
                break;
        }
        ++idx;
    }
}

void GraphicsManager::updateVoxelPass(const Scene& p_scene) {
    fillPass(
        p_scene,
        voxel_pass,
        [](const ObjectComponent& object) {
            return object.flags & ObjectComponent::RENDERABLE;
        },
        [&](const AABB& aabb) {
            unused(aabb);
            // return scene->get_bound().intersects(aabb);
            return true;
        });
}

void GraphicsManager::updateMainPass(const Scene& p_scene) {
    const Camera& camera = *p_scene.m_camera;
    Frustum camera_frustum(camera.getProjectionViewMatrix());

    // main pass
    PerPassConstantBuffer pass_constant;
    pass_constant.g_view_matrix = camera.getViewMatrix();

    const float fovy = camera.getFovy().toRad();
    const float aspect = camera.getAspect();
    if (getBackend() == Backend::OPENGL) {
        pass_constant.g_projection_matrix = buildOpenGLPerspectiveRH(fovy, aspect, camera.getNear(), camera.getFar());
    } else {
        pass_constant.g_projection_matrix = buildPerspectiveRH(fovy, aspect, camera.getNear(), camera.getFar());
    }

    main_pass.pass_idx = static_cast<int>(m_context.pass_cache.size());
    m_context.pass_cache.emplace_back(pass_constant);

    fillPass(
        p_scene,
        main_pass,
        [](const ObjectComponent& object) {
            return object.flags & ObjectComponent::RENDERABLE;
        },
        [&](const AABB& aabb) {
            return camera_frustum.intersects(aabb);
        });
}

void GraphicsManager::fillPass(const Scene& p_scene, PassContext& p_pass, FilterObjectFunc1 p_filter1, FilterObjectFunc2 p_filter2) {
    for (auto [entity, obj] : p_scene.m_ObjectComponents) {
        if (!p_scene.contains<TransformComponent>(entity)) {
            continue;
        }

        if (!p_filter1(obj)) {
            continue;
        }

        const TransformComponent& transform = *p_scene.getComponent<TransformComponent>(entity);
        DEV_ASSERT(p_scene.contains<MeshComponent>(obj.mesh_id));
        const MeshComponent& mesh = *p_scene.getComponent<MeshComponent>(obj.mesh_id);

        const mat4& world_matrix = transform.getWorldMatrix();
        AABB aabb = mesh.local_bound;
        aabb.applyMatrix(world_matrix);
        if (!p_filter2(aabb)) {
            continue;
        }

        PerBatchConstantBuffer batch_buffer;
        batch_buffer.u_world_matrix = world_matrix;

        BatchContext draw;
        draw.flags = 0;
        if (entity == p_scene.m_selected) {
            draw.flags |= STENCIL_FLAG_SELECTED;
        }

        draw.batch_idx = m_context.batch_cache.findOrAdd(entity, batch_buffer);
        if (mesh.armature_id.isValid()) {
            auto& armature = *p_scene.getComponent<ArmatureComponent>(mesh.armature_id);
            DEV_ASSERT(armature.bone_transforms.size() <= MAX_BONE_COUNT);

            BoneConstantBuffer bone;
            memcpy(bone.u_bones, armature.bone_transforms.data(), sizeof(mat4) * armature.bone_transforms.size());

            // @TODO: better memory usage
            draw.bone_idx = m_context.bone_cache.findOrAdd(mesh.armature_id, bone);
        } else {
            draw.bone_idx = -1;
        }
        DEV_ASSERT(mesh.gpu_resource);
        draw.mesh_data = (MeshBuffers*)mesh.gpu_resource;

        for (const auto& subset : mesh.subsets) {
            aabb = subset.local_bound;
            aabb.applyMatrix(world_matrix);
            if (!p_filter2(aabb)) {
                continue;
            }

            const MaterialComponent* material = p_scene.getComponent<MaterialComponent>(subset.material_id);
            MaterialConstantBuffer material_buffer;
            fill_material_constant_buffer(material, material_buffer);

            DrawContext sub_mesh;
            sub_mesh.index_count = subset.index_count;
            sub_mesh.index_offset = subset.index_offset;
            sub_mesh.material_idx = m_context.material_cache.findOrAdd(subset.material_id, material_buffer);

            draw.subsets.emplace_back(std::move(sub_mesh));
        }

        p_pass.draws.emplace_back(std::move(draw));
    }
}

}  // namespace my
