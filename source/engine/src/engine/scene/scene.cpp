#include "Scene.h"

#include "core/io/archive.h"
#include "core/systems/job_system.h"

namespace my {

using ecs::Entity;
using jobsystem::Context;

// static constexpr uint32_t kSmallSubtaskGroupSize = 64;
static constexpr uint32_t kSmallSubtaskGroupSize = 64;
// kSceneVersion history
// version 2: don't serialize scene.m_bound
// version 3: light component atten
// version 4: light component flags
// version 5: add validation
// version 6: add collider component
static constexpr uint32_t kSceneVersion = 6;
static constexpr uint32_t kSceneMagicNumber = 'xScn';

// @TODO: refactor
#if 1
#define JS_PARALLEL_FOR(CTX, INDEX, COUNT, SUBCOUNT, BODY) \
    CTX.dispatch(                                          \
        static_cast<uint32_t>(COUNT),                      \
        SUBCOUNT,                                          \
        [&](jobsystem::JobArgs args) { const uint32_t INDEX = args.job_index; do { BODY; } while(0); })
#else
#define JS_PARALLEL_FOR(CTX, INDEX, COUNT, SUBCOUNT, BODY)                    \
    (void)(CTX);                                                              \
    for (uint32_t INDEX = 0; INDEX < static_cast<uint32_t>(COUNT); ++INDEX) { \
        BODY;                                                                 \
    }
#endif

void Scene::update(float dt) {
    m_delta_time = dt;

    Context ctx;

    // animation
    run_light_update_system(ctx);
    run_animation_update_system(ctx);
    ctx.wait();
    // transform, update local matrix from position, rotation and scale
    run_transformation_update_system(ctx);
    ctx.wait();
    // hierarchy, update world matrix based on hierarchy
    run_hierarchy_update_system(ctx);
    ctx.wait();
    // armature
    run_armature_update_system(ctx);
    ctx.wait();

    // update bounding box
    run_object_update_system(ctx);

    if (m_camera) {
        m_camera->update();
    }
}

void Scene::merge(Scene& other) {
    for (auto& entry : m_component_lib.m_entries) {
        entry.second.m_manager->merge(*other.m_component_lib.m_entries[entry.first].m_manager);
    }
    if (other.m_root.is_valid()) {
        attach_component(other.m_root, m_root);
    }

    m_bound.union_box(other.m_bound);
}

void Scene::create_camera(int width,
                          int height,
                          float near_plane,
                          float far_plane,
                          Degree fovy) {
    m_camera = std::make_shared<Camera>();
    m_camera->m_width = width;
    m_camera->m_height = height;
    m_camera->m_near = near_plane;
    m_camera->m_far = far_plane;
    m_camera->m_fovy = fovy;
    m_camera->m_pitch = Degree{ -10.0f };
    m_camera->m_yaw = Degree{ -90.0f };
    m_camera->m_position = vec3{ 0, 2, 6 };
    m_camera->set_dirty();
}

Entity Scene::create_name_entity(const std::string& name) {
    Entity entity = Entity::create();
    create<NameComponent>(entity).set_name(name);
    return entity;
}

Entity Scene::create_transform_entity(const std::string& name) {
    Entity entity = create_name_entity(name);
    create<TransformComponent>(entity);
    return entity;
}

Entity Scene::create_object_entity(const std::string& name) {
    Entity entity = create_name_entity(name);
    create<ObjectComponent>(entity);
    create<TransformComponent>(entity);
    return entity;
}

Entity Scene::create_mesh_entity(const std::string& name) {
    Entity entity = create_name_entity(name);
    create<MeshComponent>(entity);
    return entity;
}

Entity Scene::create_material_entity(const std::string& name) {
    Entity entity = create_name_entity(name);
    create<MaterialComponent>(entity);
    return entity;
}

Entity Scene::create_box_selectable(const std::string& name, const AABB& aabb) {
    Entity entity = create_name_entity(name);
    BoxColliderComponent& collider = create<BoxColliderComponent>(entity);
    collider.box = aabb;
    create<SelectableComponent>(entity);
    return entity;
}

Entity Scene::create_pointlight_entity(const std::string& name, const vec3& position, const vec3& color,
                                       const float energy) {
    Entity entity = create_box_selectable(name, AABB::from_center_size(vec3(0), vec3(0.3f)));

    TransformComponent& transform = create<TransformComponent>(entity);
    transform.set_translation(position);
    transform.set_dirty();

    LightComponent& light = create<LightComponent>(entity);
    light.type = LIGHT_TYPE_POINT;
    light.color = color;
    light.energy = energy;
    light.atten.constant = 1.0f;
    light.atten.linear = 0.09f;
    light.atten.quadratic = 0.032f;
    return entity;
}

Entity Scene::create_omnilight_entity(const std::string& name, const vec3& color, const float energy) {
    Entity entity = create_box_selectable(name, AABB::from_center_size(vec3(0), vec3(0.3f)));

    create<TransformComponent>(entity);

    LightComponent& light = create<LightComponent>(entity);
    light.type = LIGHT_TYPE_OMNI;
    light.color = color;
    light.energy = energy;
    light.atten.constant = 1.0f;
    light.atten.linear = 0.0f;
    light.atten.quadratic = 0.0f;
    return entity;
}

Entity Scene::create_sphere_entity(const std::string& name, float radius, const mat4& transform) {
    Entity matID = create_material_entity(name + ":mat");
    return create_sphere_entity(name, matID, radius, transform);
}

Entity Scene::create_sphere_entity(const std::string& name, Entity material_id, float radius,
                                   const mat4& transform) {
    CRASH_NOW();
    (void)name;
    (void)material_id;
    (void)radius;
    (void)transform;
    Entity entity = create_object_entity(name);
    // TransformComponent& trans = *get_component<TransformComponent>(entity);
    // ObjectComponent& obj = *get_component<ObjectComponent>(entity);
    // trans.MatrixTransform(transform);

    // Entity meshID = Entity_CreateMesh(name + ":mesh");
    // obj.meshID = meshID;

    // MeshComponent& mesh = *get_component<MeshComponent>(meshID);

    // fill_sphere_data(mesh, radius);

    // MeshComponent::MeshSubset subset;
    // subset.materialID = materialID;
    // subset.indexCount = static_cast<uint32_t>(mesh.mIndices.size());
    // subset.indexOffset = 0;
    // mesh.mSubsets.emplace_back(subset);

    return entity;
}

Entity Scene::create_cube_entity(const std::string& name, const vec3& scale, const mat4& transform) {
    Entity material_id = create_material_entity(name + ":mat");
    return create_cube_entity(name, material_id, scale, transform);
}

Entity Scene::create_cube_entity(const std::string& name, Entity material_id, const vec3& scale,
                                 const mat4& transform) {
    ecs::Entity entity = create_object_entity(name);
    TransformComponent& trans = *get_component<TransformComponent>(entity);
    ObjectComponent& obj = *get_component<ObjectComponent>(entity);
    trans.matrix_transform(transform);

    ecs::Entity mesh_id = create_mesh_entity(name + ":mesh");
    obj.mesh_id = mesh_id;

    MeshComponent& mesh = *get_component<MeshComponent>(mesh_id);

    // @TODO: move it to somewhere else
    // clang-format off
    constexpr uint32_t indices[] = {
        0,          1,          2,          0,          2,          3,
        0 + 4,      2 + 4,      1 + 4,      0 + 4,      3 + 4,      2 + 4,  // swapped winding
        0 + 4 * 2,  1 + 4 * 2,  2 + 4 * 2,  0 + 4 * 2,  2 + 4 * 2,  3 + 4 * 2,
        0 + 4 * 3,  2 + 4 * 3,  1 + 4 * 3,  0 + 4 * 3,  3 + 4 * 3,  2 + 4 * 3, // swapped winding
        0 + 4 * 4,  2 + 4 * 4,  1 + 4 * 4,  0 + 4 * 4,  3 + 4 * 4,  2 + 4 * 4, // swapped winding
        0 + 4 * 5,  1 + 4 * 5,  2 + 4 * 5,  0 + 4 * 5,  2 + 4 * 5,  3 + 4 * 5,
    };
    // clang-format on

    const vec3& s = scale;
    mesh.positions = {
        // -Z
        vec3(-s.x, +s.y, -s.z),
        vec3(-s.x, -s.y, -s.z),
        vec3(+s.x, -s.y, -s.z),
        vec3(+s.x, +s.y, -s.z),

        // +Z
        vec3(-s.x, +s.y, +s.z),
        vec3(-s.x, -s.y, +s.z),
        vec3(+s.x, -s.y, +s.z),
        vec3(+s.x, +s.y, +s.z),

        // -X
        vec3(-s.x, -s.y, +s.z),
        vec3(-s.x, -s.y, -s.z),
        vec3(-s.x, +s.y, -s.z),
        vec3(-s.x, +s.y, +s.z),

        // +X
        vec3(+s.x, -s.y, +s.z),
        vec3(+s.x, -s.y, -s.z),
        vec3(+s.x, +s.y, -s.z),
        vec3(+s.x, +s.y, +s.z),

        // -Y
        vec3(-s.x, -s.y, +s.z),
        vec3(-s.x, -s.y, -s.z),
        vec3(+s.x, -s.y, -s.z),
        vec3(+s.x, -s.y, +s.z),

        // +Y
        vec3(-s.x, +s.y, +s.z),
        vec3(-s.x, +s.y, -s.z),
        vec3(+s.x, +s.y, -s.z),
        vec3(+s.x, +s.y, +s.z),
    };

    mesh.texcoords_0 = {
        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 1),
        vec2(1, 0),

        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 1),
        vec2(1, 0),

        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 1),
        vec2(1, 0),

        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 1),
        vec2(1, 0),

        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 1),
        vec2(1, 0),

        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 1),
        vec2(1, 0),
    };

    mesh.normals = {
        vec3(0, 0, -1),
        vec3(0, 0, -1),
        vec3(0, 0, -1),
        vec3(0, 0, -1),

        vec3(0, 0, 1),
        vec3(0, 0, 1),
        vec3(0, 0, 1),
        vec3(0, 0, 1),

        vec3(-1, 0, 0),
        vec3(-1, 0, 0),
        vec3(-1, 0, 0),
        vec3(-1, 0, 0),

        vec3(1, 0, 0),
        vec3(1, 0, 0),
        vec3(1, 0, 0),
        vec3(1, 0, 0),

        vec3(0, -1, 0),
        vec3(0, -1, 0),
        vec3(0, -1, 0),
        vec3(0, -1, 0),

        vec3(0, 1, 0),
        vec3(0, 1, 0),
        vec3(0, 1, 0),
        vec3(0, 1, 0),
    };
    MeshComponent::MeshSubset subset;
    subset.material_id = material_id;
    subset.index_count = array_length(indices);
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    for (int i = 0; i < array_length(indices); i += 3) {
        mesh.indices.push_back(indices[i]);
        mesh.indices.push_back(indices[i + 2]);
        mesh.indices.push_back(indices[i + 1]);
    }

    mesh.create_render_data();
    return entity;
}

void Scene::attach_component(Entity child, Entity parent) {
    DEV_ASSERT(child != parent);
    DEV_ASSERT(parent.is_valid());

    // if child already has a parent, detach it
    if (contains<HierarchyComponent>(child)) {
        CRASH_NOW_MSG("Unlikely to happen at this point");
    }

    HierarchyComponent& hier = create<HierarchyComponent>(child);
    hier.m_parent_id = parent;
}

void Scene::remove_entity(Entity entity) {
    m_HierarchyComponents.remove(entity);
    m_TransformComponents.remove(entity);
    m_ObjectComponents.remove(entity);
}

void Scene::update_animation(uint32_t index) {
    AnimationComponent& animation = m_AnimationComponents[index];
    if (!animation.is_playing()) {
        return;
    }

    for (const AnimationComponent::Channel& channel : animation.channels) {
        if (channel.path == AnimationComponent::Channel::PATH_UNKNOWN) {
            continue;
        }
        DEV_ASSERT(channel.sampler_index < (int)animation.samplers.size());
        const AnimationComponent::Sampler& sampler = animation.samplers[channel.sampler_index];

        int key_left = 0;
        int key_right = 0;
        float time_first = std::numeric_limits<float>::min();
        float time_last = std::numeric_limits<float>::min();
        float time_left = std::numeric_limits<float>::min();
        float time_right = std::numeric_limits<float>::max();

        for (int k = 0; k < (int)sampler.keyframe_times.size(); ++k) {
            const float time = sampler.keyframe_times[k];
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

        const float left = sampler.keyframe_times[key_left];
        const float right = sampler.keyframe_times[key_right];

        float t = 0;
        if (key_left != key_right) {
            t = (animation.timer - left) / (right - left);
        }
        t = saturate(t);

        TransformComponent* targetTransform = get_component<TransformComponent>(channel.target_id);
        DEV_ASSERT(targetTransform);
        switch (channel.path) {
            case AnimationComponent::Channel::PATH_SCALE: {
                DEV_ASSERT(sampler.keyframe_data.size() == sampler.keyframe_times.size() * 3);
                const vec3* data = (const vec3*)sampler.keyframe_data.data();
                const vec3& vLeft = data[key_left];
                const vec3& vRight = data[key_right];
                targetTransform->set_scale(glm::mix(vLeft, vRight, t));
                break;
            }
            case AnimationComponent::Channel::PATH_TRANSLATION: {
                DEV_ASSERT(sampler.keyframe_data.size() == sampler.keyframe_times.size() * 3);
                const vec3* data = (const vec3*)sampler.keyframe_data.data();
                const vec3& vLeft = data[key_left];
                const vec3& vRight = data[key_right];
                targetTransform->set_translation(glm::mix(vLeft, vRight, t));
                break;
            }
            case AnimationComponent::Channel::PATH_ROTATION: {
                DEV_ASSERT(sampler.keyframe_data.size() == sampler.keyframe_times.size() * 4);
                const vec4* data = (const vec4*)sampler.keyframe_data.data();
                const vec4& vLeft = data[key_left];
                const vec4& vRight = data[key_right];
                targetTransform->set_rotation(glm::mix(vLeft, vRight, t));
                break;
            }
            default:
                CRASH_NOW();
                break;
        }
        targetTransform->set_dirty();
    }

    if (animation.is_looped() && animation.timer > animation.end) {
        animation.timer = animation.start;
    }

    if (animation.is_playing()) {
        // @TODO: set delta time
        animation.timer += m_delta_time * animation.speed;
    }
}

void Scene::update_hierarchy(uint32_t index) {
    Entity self_id = get_entity<HierarchyComponent>(index);
    TransformComponent* self_transform = get_component<TransformComponent>(self_id);

    if (!self_transform) {
        return;
    }

    mat4 world_matrix = self_transform->get_local_matrix();
    const HierarchyComponent* hierarchy = &m_HierarchyComponents[index];
    Entity parent = hierarchy->m_parent_id;

    while (parent.is_valid()) {
        TransformComponent* parent_transform = get_component<TransformComponent>(parent);
        DEV_ASSERT(parent_transform);
        world_matrix = parent_transform->get_local_matrix() * world_matrix;

        if ((hierarchy = get_component<HierarchyComponent>(parent)) != nullptr) {
            parent = hierarchy->m_parent_id;
            DEV_ASSERT(parent.is_valid());
        } else {
            parent.make_invalid();
        }
    }

    self_transform->set_world_matrix(world_matrix);
    self_transform->set_dirty(false);
}

void Scene::update_light(uint32_t index) {
    Entity self_id = get_entity<LightComponent>(index);
    LightComponent& light = m_LightComponents[index];

    constexpr float atten_factor_inv = 1.0f / 0.01f;
    if (light.atten.linear == 0.0f && light.atten.quadratic == 0.0f) {
        light.max_distance = 1000.0f;
    } else {
        // (constant + linear * x + quad * x^2) * atten_factor = 1
        // quad * x^2 + linear * x + constant - 1.0 / atten_factor = 0
        const float a = light.atten.quadratic;
        const float b = light.atten.linear;
        const float c = light.atten.constant - atten_factor_inv;

        float discriminant = b * b - 4 * a * c;
        if (discriminant < 0.0f) {
            __debugbreak();
        }

        float sqrt_d = glm::sqrt(discriminant);
        float root1 = (-b + sqrt_d) / (2 * a);
        float root2 = (-b - sqrt_d) / (2 * a);
        light.max_distance = root1 > 0.0f ? root1 : root2;
        light.max_distance = glm::max(LIGHT_SHADOW_MIN_DISTANCE + 1.0f, light.max_distance);
    }
}

void Scene::update_armature(uint32_t index) {
    Entity id = m_ArmatureComponents.get_entity(index);
    ArmatureComponent& armature = m_ArmatureComponents[index];
    TransformComponent* transform = get_component<TransformComponent>(id);
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

    const mat4 R = glm::inverse(transform->get_world_matrix());
    const size_t numBones = armature.bone_collection.size();
    if (armature.bone_transforms.size() != numBones) {
        armature.bone_transforms.resize(numBones);
    }

    int idx = 0;
    for (Entity boneID : armature.bone_collection) {
        const TransformComponent* boneTransform = get_component<TransformComponent>(boneID);
        DEV_ASSERT(boneTransform);

        const mat4& B = armature.inverse_bind_matrices[idx];
        const mat4& W = boneTransform->get_world_matrix();
        const mat4 M = R * W * B;
        armature.bone_transforms[idx] = M;
        ++idx;

        // @TODO: armature animation
    }
};

bool Scene::serialize(Archive& archive) {
    uint32_t version = UINT_MAX;
    bool is_read_mode = !archive.is_write_mode();
    if (is_read_mode) {
        uint32_t magic;
        uint32_t seed = Entity::MAX_ID;

        archive >> magic;
        ERR_FAIL_COND_V_MSG(magic != kSceneMagicNumber, false, "file corrupted");
        archive >> version;
        ERR_FAIL_COND_V_MSG(version > kSceneMagicNumber, false, std::format("file version {} is greater than max version {}", version, kSceneVersion));
        archive >> seed;
        Entity::set_seed(seed);

    } else {
        archive << kSceneMagicNumber;
        archive << kSceneVersion;
        archive << Entity::get_seed();
    }

    m_root.serialize(archive);
    if (is_read_mode) {
        m_camera = std::make_shared<Camera>();
    }
    m_camera->serialize(archive, version);

    bool ok = true;
    ok = ok && serialize<NameComponent>(archive, version);
    ok = ok && serialize<TransformComponent>(archive, version);
    ok = ok && serialize<HierarchyComponent>(archive, version);
    ok = ok && serialize<MaterialComponent>(archive, version);
    ok = ok && serialize<MeshComponent>(archive, version);
    ok = ok && serialize<ObjectComponent>(archive, version);
    ok = ok && serialize<LightComponent>(archive, version);
    ok = ok && serialize<ArmatureComponent>(archive, version);
    ok = ok && serialize<AnimationComponent>(archive, version);
    ok = ok && serialize<RigidBodyComponent>(archive, version);

    if (archive.is_write_mode() || version >= 6) {
        ok = ok && serialize<SelectableComponent>(archive, version);
        ok = ok && serialize<BoxColliderComponent>(archive, version);
        ok = ok && serialize<MeshColliderComponent>(archive, version);
    }
    return ok;
}

Scene::RayIntersectionResult Scene::select(Ray& ray) {
    RayIntersectionResult result;

    for (size_t idx = 0; idx < get_count<SelectableComponent>(); ++idx) {
        Entity entity = get_entity<SelectableComponent>(idx);
        const TransformComponent* transform = get_component<TransformComponent>(entity);
        if (!transform) {
            continue;
        }

        if (const auto* collider = get_component<BoxColliderComponent>(entity); collider) {
            mat4 inversed_model = glm::inverse(transform->get_world_matrix());
            Ray inversed_ray = ray.inverse(inversed_model);
            if (inversed_ray.intersects(collider->box)) {
                result.entity = entity;
                ray.copy_dist(inversed_ray);
            }
            continue;
        }

        if (const auto* collider = get_component<MeshColliderComponent>(entity); collider) {
            if (ray_object_intersect(collider->object_id, ray)) {
                result.entity = entity;
            }
            continue;
        }
        CRASH_NOW_MSG("????");
    }
    return result;
}

bool Scene::ray_object_intersect(Entity object_id, Ray& ray) {
    ObjectComponent* object = get_component<ObjectComponent>(object_id);
    MeshComponent* mesh = get_component<MeshComponent>(object->mesh_id);
    TransformComponent* transform = get_component<TransformComponent>(object_id);
    DEV_ASSERT(mesh && transform);

    if (!transform || !mesh) {
        return false;
    }

    mat4 inversedModel = glm::inverse(transform->get_world_matrix());
    Ray inversedRay = ray.inverse(inversedModel);
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
            ray.copy_dist(inversedRay);
            return true;
        }
    }
    return false;
}

Scene::RayIntersectionResult Scene::intersects(Ray& ray) {
    RayIntersectionResult result;

    // @TODO: box collider
    for (int object_idx = 0; object_idx < get_count<ObjectComponent>(); ++object_idx) {
        Entity entity = get_entity<ObjectComponent>(object_idx);
        if (ray_object_intersect(entity, ray)) {
            result.entity = entity;
        }
    }

    return result;
}

void Scene::run_light_update_system(Context& ctx) {
    JS_PARALLEL_FOR(ctx, index, get_count<LightComponent>(), kSmallSubtaskGroupSize, update_light(index));
}

void Scene::run_transformation_update_system(Context& ctx) {
    JS_PARALLEL_FOR(ctx, index, get_count<TransformComponent>(), kSmallSubtaskGroupSize, m_TransformComponents[index].update_transform());
    // JS_PARALLEL_FOR(ctx, index, get_count<TransformComponent>(), kSmallSubtaskGroupSize, get_component_array<TransformComponent>()[index].update_transform());
}

void Scene::run_animation_update_system(Context& ctx) {
    JS_PARALLEL_FOR(ctx, index, get_count<AnimationComponent>(), 1, update_animation(index));
}

void Scene::run_armature_update_system(Context& ctx) {
    JS_PARALLEL_FOR(ctx, index, get_count<ArmatureComponent>(), 1, update_armature(index));
}

void Scene::run_hierarchy_update_system(Context& ctx) {
    JS_PARALLEL_FOR(ctx, index, get_count<HierarchyComponent>(), kSmallSubtaskGroupSize, update_hierarchy(index));
}

void Scene::run_object_update_system(jobsystem::Context&) {
    m_bound.make_invalid();

    const uint32_t num_object = (uint32_t)get_count<ObjectComponent>();
    for (uint32_t i = 0; i < num_object; ++i) {
        ecs::Entity entity = get_entity<ObjectComponent>(i);
        if (!contains<TransformComponent>(entity)) {
            continue;
        }

        const ObjectComponent& obj = get_component_array<ObjectComponent>()[i];
        const TransformComponent& transform = *get_component<TransformComponent>(entity);
        DEV_ASSERT(contains<MeshComponent>(obj.mesh_id));
        const MeshComponent& mesh = *get_component<MeshComponent>(obj.mesh_id);

        mat4 M = transform.get_world_matrix();
        AABB aabb = mesh.local_bound;
        aabb.apply_matrix(M);
        m_bound.union_box(aabb);
    }
}

}  // namespace my