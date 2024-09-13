#include "render_data.h"

#include "core/base/rid_owner.h"
#include "core/debugger/profiler.h"
#include "core/framework/asset_manager.h"
#include "core/framework/common_dvars.h"
#include "core/framework/graphics_manager.h"
#include "core/math/frustum.h"
#include "rendering/render_manager.h"
#include "scene/scene.h"

namespace my {

void RenderData::Pass::fill_perpass(PerPassConstantBuffer& buffer) const {
    static const mat4 fixup = mat4({ 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0.5, 0 }, { 0, 0, 0, 1 }) * mat4({ 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 1, 1 });

    buffer.u_view_matrix = view_matrix;

    if (GraphicsManager::singleton().get_backend() == Backend::D3D11) {
        buffer.u_proj_matrix = fixup * projection_matrix;
        buffer.u_proj_view_matrix = fixup * projection_view_matrix;
    } else {
        buffer.u_proj_matrix = projection_matrix;
        buffer.u_proj_view_matrix = projection_view_matrix;
    }
}

template<typename T>
static auto create_uniform(GraphicsManager& p_graphics_manager, uint32_t p_max_count) {
    static_assert(sizeof(T) % 256 == 0);
    return p_graphics_manager.uniform_create(T::get_uniform_buffer_slot(), sizeof(T) * p_max_count);
}

UniformBuffer<PerPassConstantBuffer> g_per_pass_cache;
UniformBuffer<PerFrameConstantBuffer> g_per_frame_cache;
UniformBuffer<PerSceneConstantBuffer> g_constantCache;
UniformBuffer<DebugDrawConstantBuffer> g_debug_draw_cache;

template<typename T>
static void create_uniform_buffer(UniformBuffer<T>& p_buffer) {
    constexpr int slot = T::get_uniform_buffer_slot();
    p_buffer.buffer = GraphicsManager::singleton().uniform_create(slot, sizeof(T));
}

RenderData::RenderData() {
    auto& gm = GraphicsManager::singleton();
    m_batch_uniform = create_uniform<PerBatchConstantBuffer>(gm, 4096 * 16);
    m_material_uniform = create_uniform<MaterialConstantBuffer>(gm, 2048 * 16);
    m_bone_uniform = create_uniform<BoneConstantBuffer>(gm, 16);

    create_uniform_buffer<PerPassConstantBuffer>(g_per_pass_cache);
    create_uniform_buffer<PerFrameConstantBuffer>(g_per_frame_cache);
    create_uniform_buffer<PerSceneConstantBuffer>(g_constantCache);
    create_uniform_buffer<DebugDrawConstantBuffer>(g_debug_draw_cache);
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

void RenderData::clear() {
    scene = nullptr;

    for (auto& pass : shadow_passes) {
        pass.clear();
    }

    for (auto& pass : point_shadow_passes) {
        pass.reset();
    }

    main_pass.clear();
    voxel_pass.clear();
}

void RenderData::point_light_draw_data() {
    for (auto [light_id, light] : scene->m_LightComponents) {
        if (light.getType() != LIGHT_TYPE_POINT || !light.castShadow()) {
            continue;
        }

        const int light_pass_index = light.getShadowMapIndex();
        if (light_pass_index == INVALID_POINT_SHADOW_HANDLE) {
            continue;
        }

        const TransformComponent* transform = scene->getComponent<TransformComponent>(light_id);
        DEV_ASSERT(transform);
        vec3 position = transform->getTranslation();

        const auto& light_space_matrices = light.getMatrices();
        std::array<Frustum, 6> frustums = {
            Frustum{ light_space_matrices[0] },
            Frustum{ light_space_matrices[1] },
            Frustum{ light_space_matrices[2] },
            Frustum{ light_space_matrices[3] },
            Frustum{ light_space_matrices[4] },
            Frustum{ light_space_matrices[5] },
        };

        auto pass = std::make_unique<Pass>();
        fill(
            scene,
            *pass.get(),
            [](const ObjectComponent& object) {
                return object.flags & ObjectComponent::CAST_SHADOW;
            },
            [&](const AABB& aabb) {
                for (const auto& frustum : frustums) {
                    if (frustum.intersects(aabb)) {
                        return true;
                    }
                }
                return false;
            });

        DEV_ASSERT_INDEX(light_pass_index, MAX_LIGHT_CAST_SHADOW_COUNT);
        pass->light_component = light;

        point_shadow_passes[light_pass_index] = std::move(pass);
    }
}

void RenderData::update(const Scene* p_scene) {
    OPTICK_EVENT();

    m_batch_buffer_lookup.clear();
    m_batch_buffer_lookup.reserve(p_scene->getCount<TransformComponent>());
    m_batch_buffers.clear();
    m_batch_buffers.reserve(p_scene->getCount<TransformComponent>());

    m_material_buffer_lookup.clear();
    m_material_buffer_lookup.reserve(p_scene->getCount<MaterialComponent>());
    m_material_buffers.clear();
    m_material_buffers.reserve(p_scene->getCount<MaterialComponent>());

    m_bone_buffer_lookup.clear();
    m_bone_buffer_lookup.reserve(p_scene->getCount<ArmatureComponent>());
    m_bone_buffers.clear();
    m_bone_buffers.reserve(p_scene->getCount<ArmatureComponent>());

    // clean up
    clear();
    scene = p_scene;

    point_light_draw_data();

    // @TODO: generalize this
    has_sun_light = false;
    for (auto [entity, light] : p_scene->m_LightComponents) {
        if (light.getType() == LIGHT_TYPE_OMNI) {
            has_sun_light = true;
            break;
        }
    }
    if (has_sun_light) {
        // cascaded shadow map
        for (int i = 0; i < MAX_CASCADE_COUNT; ++i) {
            // @TODO: fix this
            mat4 light_matrix = g_per_frame_cache.cache.u_main_light_matrices[i];
            Frustum light_frustum(light_matrix);
            shadow_passes[i].projection_view_matrix = light_matrix;
            fill(
                p_scene,
                shadow_passes[i],
                [](const ObjectComponent& object) {
                    return object.flags & ObjectComponent::CAST_SHADOW;
                },
                [&](const AABB& aabb) {
                    return light_frustum.intersects(aabb);
                });
        }
    }

    // voxel pass
    fill(
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

    mat4 camera_projection_view = scene->m_camera->getProjectionViewMatrix();
    Frustum camera_frustum(camera_projection_view);
    // main pass
    main_pass.projection_matrix = scene->m_camera->getProjectionMatrix();
    main_pass.view_matrix = scene->m_camera->getViewMatrix();
    main_pass.projection_view_matrix = scene->m_camera->getProjectionViewMatrix();
    fill(
        p_scene,
        main_pass,
        [](const ObjectComponent& object) {
            return object.flags & ObjectComponent::RENDERABLE;
        },
        [&](const AABB& aabb) {
            return camera_frustum.intersects(aabb);
        });

    m_batch_buffer_lookup.clear();
    m_material_buffer_lookup.clear();
    // copy buffers

    GraphicsManager::singleton().uniform_update(m_batch_uniform.get(), m_batch_buffers);
    GraphicsManager::singleton().uniform_update(m_material_uniform.get(), m_material_buffers);
    GraphicsManager::singleton().uniform_update(m_bone_uniform.get(), m_bone_buffers);
}

void RenderData::fill(const Scene* p_scene, Pass& pass, FilterObjectFunc1 func1, FilterObjectFunc2 func2) {
    OPTICK_EVENT();
    scene = p_scene;

    for (auto [entity, obj] : scene->m_ObjectComponents) {
        if (!scene->contains<TransformComponent>(entity)) {
            continue;
        }

        if (!func1(obj)) {
            continue;
        }

        const TransformComponent& transform = *scene->getComponent<TransformComponent>(entity);
        DEV_ASSERT(scene->contains<MeshComponent>(obj.mesh_id));
        const MeshComponent& mesh = *scene->getComponent<MeshComponent>(obj.mesh_id);

        const mat4& world_matrix = transform.getWorldMatrix();
        AABB aabb = mesh.local_bound;
        aabb.applyMatrix(world_matrix);
        if (!func2(aabb)) {
            continue;
        }

        PerBatchConstantBuffer batch_buffer;
        batch_buffer.u_world_matrix = world_matrix;

        Mesh draw;
        draw.flags = 0;
        if (entity == scene->m_selected) {
            draw.flags |= STENCIL_FLAG_SELECTED;
        }

        draw.batch_idx = find_or_add_batch(entity, batch_buffer);
        if (mesh.armature_id.isValid()) {
            auto& armature = *scene->getComponent<ArmatureComponent>(mesh.armature_id);
            DEV_ASSERT(armature.bone_transforms.size() <= MAX_BONE_COUNT);

            BoneConstantBuffer bone;
            memcpy(bone.u_bones, armature.bone_transforms.data(), sizeof(mat4) * armature.bone_transforms.size());

            // @TODO: better memory usage
            draw.bone_idx = find_or_add_bone(mesh.armature_id, bone);
        } else {
            draw.bone_idx = -1;
        }
        DEV_ASSERT(mesh.gpu_resource);
        draw.mesh_data = (MeshBuffers*)mesh.gpu_resource;

        for (const auto& subset : mesh.subsets) {
            aabb = subset.local_bound;
            aabb.applyMatrix(world_matrix);
            if (!func2(aabb)) {
                continue;
            }

            const MaterialComponent* material = scene->getComponent<MaterialComponent>(subset.material_id);
            MaterialConstantBuffer material_buffer;
            fill_material_constant_buffer(material, material_buffer);

            SubMesh sub_mesh;
            sub_mesh.index_count = subset.index_count;
            sub_mesh.index_offset = subset.index_offset;
            sub_mesh.material_idx = find_or_add_material(subset.material_id, material_buffer);

            draw.subsets.emplace_back(std::move(sub_mesh));
        }

        pass.draws.emplace_back(std::move(draw));
    }
}

// @TODO: refactor
uint32_t RenderData::find_or_add_batch(ecs::Entity p_entity, const PerBatchConstantBuffer& p_buffer) {
    auto it = m_batch_buffer_lookup.find(p_entity);
    if (it != m_batch_buffer_lookup.end()) {
        return it->second;
    }

    uint32_t index = static_cast<uint32_t>(m_batch_buffers.size());
    m_batch_buffer_lookup[p_entity] = index;
    m_batch_buffers.emplace_back(p_buffer);
    return index;
}

uint32_t RenderData::find_or_add_material(ecs::Entity p_entity, const MaterialConstantBuffer& p_buffer) {
    auto it = m_material_buffer_lookup.find(p_entity);
    if (it != m_material_buffer_lookup.end()) {
        return it->second;
    }

    uint32_t index = static_cast<uint32_t>(m_material_buffers.size());
    m_material_buffer_lookup[p_entity] = index;
    m_material_buffers.emplace_back(p_buffer);
    return index;
}

uint32_t RenderData::find_or_add_bone(ecs::Entity p_entity, const BoneConstantBuffer& p_buffer) {
    auto it = m_bone_buffer_lookup.find(p_entity);
    if (it != m_bone_buffer_lookup.end()) {
        return it->second;
    }

    uint32_t index = static_cast<uint32_t>(m_bone_buffers.size());
    m_bone_buffer_lookup[p_entity] = index;
    m_bone_buffers.emplace_back(p_buffer);
    return index;
}

}  // namespace my
