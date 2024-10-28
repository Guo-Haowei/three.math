#include "Scene.h"

#include "core/io/archive.h"
#include "core/math/geometry.h"
#include "core/systems/job_system.h"
#include "rendering/render_manager.h"

namespace my {

using ecs::Entity;
using jobsystem::Context;

static constexpr uint32_t kSmallSubtaskGroupSize = 64;
// kSceneVersion history
// version 2: don't serialize scene.m_bound
// version 3: light component atten
// version 4: light component flags
// version 5: add validation
// version 6: add collider component
// version 7: add enabled to material
static constexpr uint32_t kSceneVersion = 7;
static constexpr uint32_t kSceneMagicNumber = 'xScn';

// @TODO: refactor
#if 1
#define JS_PARALLEL_FOR(CTX, INDEX, COUNT, SUBCOUNT, BODY) \
    CTX.Dispatch(                                          \
        static_cast<uint32_t>(COUNT),                      \
        SUBCOUNT,                                          \
        [&](jobsystem::JobArgs args) { const uint32_t INDEX = args.jobIndex; do { BODY; } while(0); })
#else
#define JS_PARALLEL_FOR(CTX, INDEX, COUNT, SUBCOUNT, BODY)                    \
    (void)(CTX);                                                              \
    for (uint32_t INDEX = 0; INDEX < static_cast<uint32_t>(COUNT); ++INDEX) { \
        BODY;                                                                 \
    }
#endif

void Scene::Update(float p_delta_time) {
    m_elapsedTime = p_delta_time;

    Context ctx;

    // animation
    RunLightUpdateSystem(ctx);
    RunAnimationUpdateSystem(ctx);
    ctx.Wait();
    // transform, update local matrix from position, rotation and scale
    RunTransformationUpdateSystem(ctx);
    // particle
    m_particleEmitter.Update(p_delta_time);
    ctx.Wait();
    // hierarchy, update world matrix based on hierarchy
    RunHierarchyUpdateSystem(ctx);
    ctx.Wait();
    // armature
    RunArmatureUpdateSystem(ctx);
    ctx.Wait();

    // update bounding box
    RunObjectUpdateSystem(ctx);

    if (m_camera) {
        m_camera->update();
    }
}

void Scene::Merge(Scene& p_other) {
    for (auto& entry : m_componentLib.m_entries) {
        entry.second.m_manager->Merge(*p_other.m_componentLib.m_entries[entry.first].m_manager);
    }
    if (p_other.m_root.IsValid()) {
        AttachComponent(p_other.m_root, m_root);
    }

    m_bound.unionBox(p_other.m_bound);
}

void Scene::CreateCamera(int p_width,
                         int p_height,
                         float p_near_plane,
                         float p_far_plane,
                         Degree p_fovy) {
    m_camera = std::make_shared<Camera>();
    m_camera->m_width = p_width;
    m_camera->m_height = p_height;
    m_camera->m_near = p_near_plane;
    m_camera->m_far = p_far_plane;
    m_camera->m_fovy = p_fovy;
    m_camera->m_pitch = Degree{ -10.0f };
    m_camera->m_yaw = Degree{ -90.0f };
    m_camera->m_position = vec3{ 0, 4, 10 };
    m_camera->setDirty();
}

Entity Scene::CreateNameEntity(const std::string& p_name) {
    Entity entity = Entity::Create();
    Create<NameComponent>(entity).SetName(p_name);
    return entity;
}

Entity Scene::CreateTransformEntity(const std::string& p_name) {
    Entity entity = CreateNameEntity(p_name);
    Create<TransformComponent>(entity);
    return entity;
}

Entity Scene::CreateObjectEntity(const std::string& p_name) {
    Entity entity = CreateNameEntity(p_name);
    Create<ObjectComponent>(entity);
    Create<TransformComponent>(entity);
    return entity;
}

Entity Scene::CreateMeshEntity(const std::string& p_name) {
    Entity entity = CreateNameEntity(p_name);
    Create<MeshComponent>(entity);
    return entity;
}

Entity Scene::CreateMaterialEntity(const std::string& p_name) {
    Entity entity = CreateNameEntity(p_name);
    Create<MaterialComponent>(entity);
    return entity;
}

Entity Scene::CreatePointLightEntity(const std::string& p_name,
                                     const vec3& p_position,
                                     const vec3& p_color,
                                     const float p_emissive) {
    Entity entity = CreateObjectEntity(p_name);

    LightComponent& light = Create<LightComponent>(entity);
    light.SetType(LIGHT_TYPE_POINT);
    light.m_atten.constant = 1.0f;
    light.m_atten.linear = 0.2f;
    light.m_atten.quadratic = 0.05f;

    MaterialComponent& material = Create<MaterialComponent>(entity);
    material.base_color = vec4(p_color, 1.0f);
    material.emissive = p_emissive;

    TransformComponent& transform = *GetComponent<TransformComponent>(entity);
    ObjectComponent& object = *GetComponent<ObjectComponent>(entity);
    transform.SetTranslation(p_position);
    transform.SetDirty();

    Entity mesh_id = CreateMeshEntity(p_name + ":mesh");
    object.meshId = mesh_id;
    object.flags = ObjectComponent::RENDERABLE;

    MeshComponent& mesh = *GetComponent<MeshComponent>(mesh_id);
    mesh = makeSphereMesh(0.1f, 40, 40);
    mesh.subsets[0].material_id = entity;
    return entity;
}

Entity Scene::CreateAreaLightEntity(const std::string& p_name,
                                    const vec3& p_color,
                                    const float p_emissive) {
    Entity entity = CreateObjectEntity(p_name);

    // light
    LightComponent& light = Create<LightComponent>(entity);
    light.SetType(LIGHT_TYPE_AREA);
    // light.m_atten.constant = 1.0f;
    // light.m_atten.linear = 0.09f;
    // light.m_atten.quadratic = 0.032f;

    // material
    MaterialComponent& material = Create<MaterialComponent>(entity);
    material.base_color = vec4(p_color, 1.0f);
    material.emissive = p_emissive;

    ObjectComponent& object = *GetComponent<ObjectComponent>(entity);

    Entity mesh_id = CreateMeshEntity(p_name + ":mesh");
    object.meshId = mesh_id;
    object.flags = ObjectComponent::RENDERABLE;

    MeshComponent& mesh = *GetComponent<MeshComponent>(mesh_id);
    mesh = makePlaneMesh();
    mesh.subsets[0].material_id = entity;
    return entity;
}

Entity Scene::CreateInfiniteLightEntity(const std::string& p_name,
                                        const vec3& p_color,
                                        const float p_emissive) {
    Entity entity = CreateNameEntity(p_name);

    Create<TransformComponent>(entity);

    LightComponent& light = Create<LightComponent>(entity);
    light.SetType(LIGHT_TYPE_INFINITE);
    light.m_atten.constant = 1.0f;
    light.m_atten.linear = 0.0f;
    light.m_atten.quadratic = 0.0f;

    MaterialComponent& material = Create<MaterialComponent>(entity);
    material.base_color = vec4(p_color, 1.0f);
    material.emissive = p_emissive;
    return entity;
}

Entity Scene::CreatePlaneEntity(const std::string& p_name,
                                const vec3& p_scale,
                                const mat4& p_transform) {
    Entity material_id = CreateMaterialEntity(p_name + ":mat");
    return CreatePlaneEntity(p_name, material_id, p_scale, p_transform);
}

Entity Scene::CreatePlaneEntity(const std::string& p_name,
                                Entity p_material_id,
                                const vec3& p_scale,
                                const mat4& p_transform) {
    ecs::Entity entity = CreateObjectEntity(p_name);
    TransformComponent& trans = *GetComponent<TransformComponent>(entity);
    ObjectComponent& object = *GetComponent<ObjectComponent>(entity);
    trans.MatrixTransform(p_transform);

    ecs::Entity mesh_id = CreateMeshEntity(p_name + ":mesh");
    object.meshId = mesh_id;

    MeshComponent& mesh = *GetComponent<MeshComponent>(mesh_id);
    mesh = makePlaneMesh(p_scale);
    mesh.subsets[0].material_id = p_material_id;

    return entity;
}

Entity Scene::CreateCubeEntity(const std::string& p_name,
                               const vec3& p_scale,
                               const mat4& p_transform) {
    Entity material_id = CreateMaterialEntity(p_name + ":mat");
    return CreateCubeEntity(p_name, material_id, p_scale, p_transform);
}

Entity Scene::CreateCubeEntity(const std::string& p_name,
                               Entity p_material_id,
                               const vec3& p_scale,
                               const mat4& p_transform) {
    ecs::Entity entity = CreateObjectEntity(p_name);
    TransformComponent& trans = *GetComponent<TransformComponent>(entity);
    ObjectComponent& object = *GetComponent<ObjectComponent>(entity);
    trans.MatrixTransform(p_transform);

    ecs::Entity mesh_id = CreateMeshEntity(p_name + ":mesh");
    object.meshId = mesh_id;

    MeshComponent& mesh = *GetComponent<MeshComponent>(mesh_id);
    mesh = makeCubeMesh(p_scale);
    mesh.subsets[0].material_id = p_material_id;

    return entity;
}

Entity Scene::CreateSphereEntity(const std::string& p_name,
                                 float p_radius,
                                 const mat4& p_transform) {
    Entity material_id = CreateMaterialEntity(p_name + ":mat");
    return CreateSphereEntity(p_name, material_id, p_radius, p_transform);
}

Entity Scene::CreateSphereEntity(const std::string& p_name,
                                 Entity p_material_id,
                                 float p_radius,
                                 const mat4& p_transform) {
    ecs::Entity entity = CreateObjectEntity(p_name);
    TransformComponent& trans = *GetComponent<TransformComponent>(entity);
    ObjectComponent& object = *GetComponent<ObjectComponent>(entity);
    trans.MatrixTransform(p_transform);

    ecs::Entity mesh_id = CreateMeshEntity(p_name + ":mesh");
    object.meshId = mesh_id;

    MeshComponent& mesh = *GetComponent<MeshComponent>(mesh_id);
    mesh = makeSphereMesh(p_radius);
    mesh.subsets[0].material_id = p_material_id;

    return entity;
}

void Scene::AttachComponent(Entity p_child, Entity p_parent) {
    DEV_ASSERT(p_child != p_parent);
    DEV_ASSERT(p_parent.IsValid());

    // if child already has a parent, detach it
    if (Contains<HierarchyComponent>(p_child)) {
        CRASH_NOW_MSG("Unlikely to happen at this point");
    }

    HierarchyComponent& hier = Create<HierarchyComponent>(p_child);
    hier.m_parentId = p_parent;
}

void Scene::RemoveEntity(Entity p_entity) {
    LightComponent* light = GetComponent<LightComponent>(p_entity);
    if (light) {
        auto shadow_handle = light->GetShadowMapIndex();
        if (shadow_handle != INVALID_POINT_SHADOW_HANDLE) {
            RenderManager::singleton().free_point_light_shadow_map(shadow_handle);
        }
        m_LightComponents.Remove(p_entity);
    }
    m_HierarchyComponents.Remove(p_entity);
    m_TransformComponents.Remove(p_entity);
    m_ObjectComponents.Remove(p_entity);
}

void Scene::UpdateLight(uint32_t p_index) {
    Entity id = GetEntity<LightComponent>(p_index);
    const TransformComponent* transform = GetComponent<TransformComponent>(id);
    DEV_ASSERT(transform);
    m_LightComponents[p_index].Update(*transform);
}

void Scene::UpdateAnimation(uint32_t p_index) {
    AnimationComponent& animation = m_AnimationComponents[p_index];
    if (!animation.isPlaying()) {
        return;
    }

    for (const AnimationComponent::Channel& channel : animation.channels) {
        if (channel.path == AnimationComponent::Channel::PATH_UNKNOWN) {
            continue;
        }
        DEV_ASSERT(channel.samplerIndex < (int)animation.samplers.size());
        const AnimationComponent::Sampler& sampler = animation.samplers[channel.samplerIndex];

        int key_left = 0;
        int key_right = 0;
        float time_first = std::numeric_limits<float>::min();
        float time_last = std::numeric_limits<float>::min();
        float time_left = std::numeric_limits<float>::min();
        float time_right = std::numeric_limits<float>::max();

        for (int k = 0; k < (int)sampler.keyframeTmes.size(); ++k) {
            const float time = sampler.keyframeTmes[k];
            if (time < time_first) {
                time_first = time;
            }
            if (time > time_last) {
                time_last = time;
            }
            if (time <= animation.timer && time > time_left) {
                time_left = time;
                key_left = k;
            }
            if (time >= animation.timer && time < time_right) {
                time_right = time;
                key_right = k;
            }
        }

        if (animation.timer < time_first) {
            continue;
        }

        const float left = sampler.keyframeTmes[key_left];
        const float right = sampler.keyframeTmes[key_right];

        float t = 0;
        if (key_left != key_right) {
            t = (animation.timer - left) / (right - left);
        }
        t = saturate(t);

        TransformComponent* targetTransform = GetComponent<TransformComponent>(channel.targetId);
        DEV_ASSERT(targetTransform);
        switch (channel.path) {
            case AnimationComponent::Channel::PATH_SCALE: {
                DEV_ASSERT(sampler.keyframeData.size() == sampler.keyframeTmes.size() * 3);
                const vec3* data = (const vec3*)sampler.keyframeData.data();
                const vec3& vLeft = data[key_left];
                const vec3& vRight = data[key_right];
                targetTransform->SetScale(glm::mix(vLeft, vRight, t));
                break;
            }
            case AnimationComponent::Channel::PATH_TRANSLATION: {
                DEV_ASSERT(sampler.keyframeData.size() == sampler.keyframeTmes.size() * 3);
                const vec3* data = (const vec3*)sampler.keyframeData.data();
                const vec3& vLeft = data[key_left];
                const vec3& vRight = data[key_right];
                targetTransform->SetTranslation(glm::mix(vLeft, vRight, t));
                break;
            }
            case AnimationComponent::Channel::PATH_ROTATION: {
                DEV_ASSERT(sampler.keyframeData.size() == sampler.keyframeTmes.size() * 4);
                const vec4* data = (const vec4*)sampler.keyframeData.data();
                const vec4& vLeft = data[key_left];
                const vec4& vRight = data[key_right];
                targetTransform->SetRotation(glm::mix(vLeft, vRight, t));
                break;
            }
            default:
                CRASH_NOW();
                break;
        }
        targetTransform->SetDirty();
    }

    if (animation.isLooped() && animation.timer > animation.end) {
        animation.timer = animation.start;
    }

    if (animation.isPlaying()) {
        // @TODO: set delta time
        animation.timer += m_elapsedTime * animation.speed;
    }
}

void Scene::UpdateHierarchy(uint32_t p_index) {
    Entity self_id = GetEntity<HierarchyComponent>(p_index);
    TransformComponent* self_transform = GetComponent<TransformComponent>(self_id);

    if (!self_transform) {
        return;
    }

    mat4 world_matrix = self_transform->GetLocalMatrix();
    const HierarchyComponent* hierarchy = &m_HierarchyComponents[p_index];
    Entity parent = hierarchy->m_parentId;

    while (parent.IsValid()) {
        TransformComponent* parent_transform = GetComponent<TransformComponent>(parent);
        DEV_ASSERT(parent_transform);
        world_matrix = parent_transform->GetLocalMatrix() * world_matrix;

        if ((hierarchy = GetComponent<HierarchyComponent>(parent)) != nullptr) {
            parent = hierarchy->m_parentId;
            DEV_ASSERT(parent.IsValid());
        } else {
            parent.MakeInvalid();
        }
    }

    self_transform->SetWorldMatrix(world_matrix);
    self_transform->SetDirty(false);
}

void Scene::UpdateArmature(uint32_t p_index) {
    Entity id = m_ArmatureComponents.GetEntity(p_index);
    ArmatureComponent& armature = m_ArmatureComponents[p_index];
    TransformComponent* transform = GetComponent<TransformComponent>(id);
    DEV_ASSERT(transform);

    // The transform world matrices are in world space, but skinning needs them in armature-local space,
    //	so that the skin is reusable for instanced meshes.
    //	We remove the armature's world matrix from the bone world matrix to obtain the bone local transform
    //	These local bone matrices will only be used for skinning, the actual transform components for the bones
    //	remain unchanged.
    //
    //	This is useful for an other thing too:
    //	If a whole transform tree is transformed by some parent (even gltf import does that to convert from RH
    // to LH space) 	then the inverseBindMatrices are not reflected in that because they are not contained in
    // the hierarchy system. 	But this will correct them too.

    const mat4 R = glm::inverse(transform->GetWorldMatrix());
    const size_t numBones = armature.boneCollection.size();
    if (armature.boneTransforms.size() != numBones) {
        armature.boneTransforms.resize(numBones);
    }

    int idx = 0;
    for (Entity boneID : armature.boneCollection) {
        const TransformComponent* boneTransform = GetComponent<TransformComponent>(boneID);
        DEV_ASSERT(boneTransform);

        const mat4& B = armature.inverseBindMatrices[idx];
        const mat4& W = boneTransform->GetWorldMatrix();
        const mat4 M = R * W * B;
        armature.boneTransforms[idx] = M;
        ++idx;

        // @TODO: armature animation
    }
};

bool Scene::Serialize(Archive& p_archive) {
    uint32_t version = UINT_MAX;
    bool is_read_mode = !p_archive.IsWriteMode();
    if (is_read_mode) {
        uint32_t magic;
        uint32_t seed = Entity::MAX_ID;

        p_archive >> magic;
        ERR_FAIL_COND_V_MSG(magic != kSceneMagicNumber, false, "file corrupted");
        p_archive >> version;
        ERR_FAIL_COND_V_MSG(version > kSceneMagicNumber, false, std::format("file version {} is greater than max version {}", version, kSceneVersion));
        p_archive >> seed;
        Entity::SetSeed(seed);

    } else {
        p_archive << kSceneMagicNumber;
        p_archive << kSceneVersion;
        p_archive << Entity::GetSeed();
    }

    m_root.Serialize(p_archive);
    if (is_read_mode) {
        m_camera = std::make_shared<Camera>();
    }
    m_camera->Serialize(p_archive, version);

    constexpr uint64_t has_next_flag = 6368519827137030510;
    if (p_archive.IsWriteMode()) {
        for (const auto& it : m_componentLib.m_entries) {
            p_archive << has_next_flag;
            p_archive << it.first;  // write name
            it.second.m_manager->Serialize(p_archive, version);
        }
        p_archive << uint64_t(0);
        return true;
    } else {
        for (;;) {
            uint64_t has_next = 0;
            p_archive >> has_next;
            if (has_next != has_next_flag) {
                return true;
            }

            std::string key;
            p_archive >> key;
            auto it = m_componentLib.m_entries.find(key);
            if (it == m_componentLib.m_entries.end()) {
                LOG_ERROR("scene corrupted");
                return false;
            }
            if (!it->second.m_manager->Serialize(p_archive, version)) {
                return false;
            }
        }
    }
}

bool Scene::RayObjectIntersect(Entity p_object_id, Ray& p_ray) {
    ObjectComponent* object = GetComponent<ObjectComponent>(p_object_id);
    MeshComponent* mesh = GetComponent<MeshComponent>(object->meshId);
    TransformComponent* transform = GetComponent<TransformComponent>(p_object_id);
    DEV_ASSERT(mesh && transform);

    if (!transform || !mesh) {
        return false;
    }

    mat4 inversedModel = glm::inverse(transform->GetWorldMatrix());
    Ray inversedRay = p_ray.inverse(inversedModel);
    Ray inversedRayAABB = inversedRay;  // make a copy, we don't want dist to be modified by AABB
    // Perform aabb test
    if (!inversedRayAABB.intersects(mesh->local_bound)) {
        return false;
    }

    // @TODO: test submesh intersection

    // Test every single triange
    for (size_t i = 0; i < mesh->indices.size(); i += 3) {
        const vec3& A = mesh->positions[mesh->indices[i]];
        const vec3& B = mesh->positions[mesh->indices[i + 1]];
        const vec3& C = mesh->positions[mesh->indices[i + 2]];
        if (inversedRay.intersects(A, B, C)) {
            p_ray.copyDist(inversedRay);
            return true;
        }
    }
    return false;
}

Scene::RayIntersectionResult Scene::Intersects(Ray& p_ray) {
    RayIntersectionResult result;

    // @TODO: box collider
    for (int object_idx = 0; object_idx < GetCount<ObjectComponent>(); ++object_idx) {
        Entity entity = GetEntity<ObjectComponent>(object_idx);
        if (RayObjectIntersect(entity, p_ray)) {
            result.entity = entity;
        }
    }

    return result;
}

void Scene::RunLightUpdateSystem(Context& p_ctx) {
    JS_PARALLEL_FOR(p_ctx, index, GetCount<LightComponent>(), kSmallSubtaskGroupSize, UpdateLight(index));
}

void Scene::RunTransformationUpdateSystem(Context& p_ctx) {
    JS_PARALLEL_FOR(p_ctx, index, GetCount<TransformComponent>(), kSmallSubtaskGroupSize, m_TransformComponents[index].UpdateTransform());
}

void Scene::RunAnimationUpdateSystem(Context& p_ctx) {
    JS_PARALLEL_FOR(p_ctx, index, GetCount<AnimationComponent>(), 1, UpdateAnimation(index));
}

void Scene::RunArmatureUpdateSystem(Context& p_ctx) {
    JS_PARALLEL_FOR(p_ctx, index, GetCount<ArmatureComponent>(), 1, UpdateArmature(index));
}

void Scene::RunHierarchyUpdateSystem(Context& p_ctx) {
    JS_PARALLEL_FOR(p_ctx, index, GetCount<HierarchyComponent>(), kSmallSubtaskGroupSize, UpdateHierarchy(index));
}

void Scene::RunObjectUpdateSystem(jobsystem::Context&) {
    m_bound.makeInvalid();

    for (auto [entity, obj] : m_ObjectComponents) {
        if (!Contains<TransformComponent>(entity)) {
            continue;
        }

        const TransformComponent& transform = *GetComponent<TransformComponent>(entity);
        DEV_ASSERT(Contains<MeshComponent>(obj.meshId));
        const MeshComponent& mesh = *GetComponent<MeshComponent>(obj.meshId);

        mat4 M = transform.GetWorldMatrix();
        AABB aabb = mesh.local_bound;
        aabb.applyMatrix(M);
        m_bound.unionBox(aabb);
    }
}

}  // namespace my