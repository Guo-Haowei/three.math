#include "lua_scene_binding.h"

#include "sol/sol.hpp"

namespace my {

using ecs::Entity;

static bool try_get_float(const sol::table& p_table, const std::string& p_key, float& p_out) {
    sol::optional<float> f = p_table[p_key];
    if (!f) {
        return false;
    }

    p_out = *f;
    return true;
}

static bool try_get_vec3(const sol::table& p_table, const std::string& p_key, vec3& p_out) {
    sol::optional<sol::table> vec_array = p_table[p_key];
    if (!vec_array) {
        return false;
    }

    sol::optional<float> x = (*vec_array)[1];
    sol::optional<float> y = (*vec_array)[2];
    sol::optional<float> z = (*vec_array)[3];
    if (!x || !y || !x) {
        return false;
    }

    p_out = vec3(*x, *y, *z);
    return true;
}

static Entity lua_scene_create_entity(Scene* p_scene, const sol::table& p_components) {
    Entity id;
    Entity material_id;

    static int s_counter = 0;
    std::string name = std::format("Untitled-{}", ++s_counter);

    if (sol::optional<std::string> optional_name = p_components["name"]; optional_name) {
        name = *optional_name;
    }

    if (sol::optional<std::string> type = p_components["type"]; type) {
        if (type == "POINT_LIGHT") {
            id = p_scene->create_pointlight_entity(name, vec3(0));
        } else {
            CRASH_NOW();
        }
    }

    auto create_material = [&]() {
        return p_scene->create_material_entity(name + "-material");
    };

    if (sol::optional<sol::table> table = p_components["material"]; table) {
        material_id = create_material();
        MaterialComponent* material = p_scene->get_component<MaterialComponent>(material_id);
        vec3 color{ 1 };
        bool ok = true;
        ok = ok && try_get_float(*table, "roughness", material->roughness);
        ok = ok && try_get_float(*table, "metallic", material->metallic);
        DEV_ASSERT(ok);
        if (try_get_vec3(*table, "base_color", color)) {
            material->base_color = vec4(color, 1.0f);
        }
    }

    if (sol::optional<sol::table> table = p_components["cube"]; table) {
        vec3 size{ 0.5f };
        try_get_vec3(*table, "size", size);
        if (!material_id.is_valid()) {
            material_id = create_material();
        }

        id = p_scene->create_cube_entity(name, material_id, size);
    }
    if (sol::optional<sol::table> table = p_components["sphere"]; table) {
        float radius{ 0.5f };
        try_get_float(*table, "radius", radius);
        if (!material_id.is_valid()) {
            material_id = create_material();
        }

        id = p_scene->create_sphere_entity(name, material_id, radius);
    }

    if (sol::optional<sol::table> table = p_components["transform"]; table) {
        vec3 translate{ 0 };
        vec3 rotate{ 0 };
        vec3 scale{ 1 };
        try_get_vec3(*table, "translate", translate);
        try_get_vec3(*table, "rotate", rotate);
        try_get_vec3(*table, "scale", scale);

        TransformComponent* transform = p_scene->get_component<TransformComponent>(id);
        if (!transform) {
            transform = &p_scene->create<TransformComponent>(id);
        }
        transform->set_translation(translate);
        // transform_component.set_rotation(rotate);
        transform->set_scale(scale);
        transform->set_dirty();
    }

    if (sol::optional<sol::table> table = p_components["rigid_body"]; table) {
        if (sol::optional<std::string> shape = (*table)["shape"]; shape) {
            auto sss = *shape;
            unused(sss);
            float mass = 0.0f;
            if (try_get_float(*table, "mass", mass)) {
                if (*shape == "SHAPE_CUBE") {
                    auto& rigid_body = p_scene->create<RigidBodyComponent>(id);
                    rigid_body.shape = RigidBodyComponent::SHAPE_CUBE;
                    rigid_body.mass = mass;
                    try_get_vec3(*table, "size", rigid_body.param.box.half_size);
                } else if (*shape == "SHAPE_SPHERE") {
                    auto& rigid_body = p_scene->create<RigidBodyComponent>(id);
                    rigid_body.shape = RigidBodyComponent::SHAPE_SPHERE;
                    rigid_body.mass = mass;
                    try_get_float(*table, "radius", rigid_body.param.sphere.radius);
                } else {
                    CRASH_NOW();
                }
            }
        }
    }

    DEV_ASSERT(id.is_valid());
    p_scene->attach_component(id, p_scene->m_root);
    return id;
}

bool load_lua_scene(const std::string& p_path, Scene* p_scene) {
    sol::state lua;
    lua.open_libraries();

    // install libs
    sol::table lib = lua.create_table();
    lua.set("Scene", lib);
    lib.set_function("get", [&]() { return p_scene; });
    lib.set_function("create_entity", lua_scene_create_entity);

    // add additional package path
    std::string package_path = lua["package"]["path"];

    // add search directory
    std::filesystem::path new_search_dir{ __FILE__ };
    new_search_dir = new_search_dir.remove_filename();
    new_search_dir.append("?.lua;");
    lua["package"]["path"] = new_search_dir.string() + package_path;

    lua.script_file(p_path);

    return true;
}

}  // namespace my
