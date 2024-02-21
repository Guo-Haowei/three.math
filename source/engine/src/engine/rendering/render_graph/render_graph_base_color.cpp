#include "render_graph_base_color.h"

// @TODO: refactor
#include "core/base/rid_owner.h"
#include "core/framework/graphics_manager.h"
#include "core/framework/scene_manager.h"
#include "core/math/frustum.h"
#include "rendering/r_cbuffers.h"
#include "rendering/render_data.h"
#include "rendering/rendering_dvars.h"

extern my::RIDAllocator<MeshData> g_meshes;

namespace my::rg {

void base_color_pass(int width, int height) {
    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto render_data = GraphicsManager::singleton().get_render_data();
    RenderData::Pass& pass = render_data->main_pass;

    for (const auto& draw : pass.draws) {
        const bool has_bone = draw.armature_id.is_valid();

        if (has_bone) {
            auto& armature = *render_data->scene->get_component<ArmatureComponent>(draw.armature_id);
            DEV_ASSERT(armature.bone_transforms.size() <= MAX_BONE_COUNT);

            memcpy(g_boneCache.cache.c_bones, armature.bone_transforms.data(), sizeof(mat4) * armature.bone_transforms.size());
            g_boneCache.Update();
        }

        GraphicsManager::singleton().set_pipeline_state(has_bone ? PROGRAM_BASE_COLOR_ANIMATED : PROGRAM_BASE_COLOR_STATIC);

        g_perBatchCache.cache.c_projection_view_model_matrix = pass.projection_view_matrix * draw.world_matrix;
        g_perBatchCache.cache.c_model_matrix = draw.world_matrix;
        g_perBatchCache.Update();

        glBindVertexArray(draw.mesh_data->vao);

        for (const auto& subset : draw.subsets) {
            GraphicsManager::singleton().fill_material_constant_buffer(subset.material, g_materialCache.cache);
            g_materialCache.Update();

            glDrawElements(GL_TRIANGLES, subset.index_count, GL_UNSIGNED_INT, (void*)(subset.index_offset * sizeof(uint32_t)));
        }
    }
}

void create_render_graph_base_color(RenderGraph& graph) {
    ivec2 frame_size = DVAR_GET_IVEC2(resolution);
    int w = frame_size.x;
    int h = frame_size.y;

    GraphicsManager& manager = GraphicsManager::singleton();

    auto color_attachment = manager.create_resource(RenderTargetDesc{ RT_RES_BASE_COLOR,
                                                                      FORMAT_R8G8B8A8_UINT,
                                                                      RT_COLOR_ATTACHMENT,
                                                                      w, h });
    auto depth_attachment = manager.create_resource(RenderTargetDesc{ RT_RES_BASE_COLOR_DEPTH,
                                                                      FORMAT_D32_FLOAT,
                                                                      RT_DEPTH_ATTACHMENT,
                                                                      w, h });

    {  // base color
        RenderPassDesc desc;
        desc.name = BASE_COLOR_PASS;
        desc.subpasses.emplace_back(SubPassDesc{
            .color_attachments = { color_attachment },
            .depth_attachment = depth_attachment,
            .func = base_color_pass,
        });
        graph.add_pass(desc);
    }

    graph.compile();
}

}  // namespace my::rg
