#pragma once
#include "core/base/fixed_stack.h"
#include "gl_utils.h"
#include "scene/scene.h"

namespace my {

struct RenderData {
    using FilterObjectFunc1 = std::function<bool(const ObjectComponent& object)>;
    using FilterObjectFunc2 = std::function<bool(const AABB& object_aabb)>;

    struct SubMesh {
        uint32_t index_count;
        uint32_t index_offset;
        const MaterialComponent* material;
        uint32_t flags;
    };

    struct Mesh {
        ecs::Entity armature_id;
        mat4 world_matrix;
        const MeshData* mesh_data;
        std::vector<SubMesh> subsets;
    };

    struct Pass {
        mat4 projection_view_matrix;
        std::vector<Mesh> draws;
        int light_index;

        void clear() { draws.clear(); }
    };

    struct LightBillboard {
        mat4 transform;
        int type;
        Image* image;
    };

    const Scene* scene = nullptr;

    // @TODO: fix this ugly shit

    // @TODO: save pass item somewhere and use index instead of keeping many copies
    std::array<Pass, MAX_CASCADE_COUNT> shadow_passes;
    FixedStack<Pass, MAX_LIGHT_CAST_SHADOW_COUNT> point_shadow_passes;

    Pass voxel_pass;
    Pass main_pass;
    // @TODO: array
    std::vector<LightBillboard> light_billboards;

    void update(const Scene* p_scene);

private:
    void clear();

    void point_light_draw_data();
    void fill(const Scene* p_scene, const mat4& projection_view_matrix, Pass& pass, FilterObjectFunc1 func1, FilterObjectFunc2 func2);
};

}  // namespace my