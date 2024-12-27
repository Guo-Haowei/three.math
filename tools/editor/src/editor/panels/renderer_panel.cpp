#include "renderer_panel.h"

#include <imgui/imgui_internal.h>

#include "engine/core/framework/common_dvars.h"
#include "engine/core/framework/graphics_manager.h"
#include "engine/renderer/graphics_dvars.h"
#include "engine/renderer/render_graph/render_graph_defines.h"
#include "engine/scene/scene.h"

namespace my {

#if 0
static void disable_widget() {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}

static void enable_widget() {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}
#endif

static void CollapseWindow(const std::string& p_window_name, std::function<void(void)>&& p_funcion) {
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                                     ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap |
                                     ImGuiTreeNodeFlags_FramePadding;
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
        ImGui::Separator();
        bool open = ImGui::TreeNodeEx(p_window_name.c_str(), flags, "%s", p_window_name.c_str());
        ImGui::PopStyleVar();

        if (open) {
            p_funcion();
            ImGui::TreePop();
        }
    }
}

void RendererPanel::UpdateInternal(Scene&) {
    ImGui::Text("Debug");
    ImGui::Text("Frame rate:%.2f", ImGui::GetIO().Framerate);
    ImGui::Checkbox("show editor", (bool*)DVAR_GET_POINTER(show_editor));
    ImGui::Checkbox("no texture", (bool*)DVAR_GET_POINTER(gfx_no_texture));

    CollapseWindow("Shadow", []() {
        ImGui::Checkbox("debug", (bool*)DVAR_GET_POINTER(gfx_debug_shadow));
    });

    CollapseWindow("Bloom", []() {
        ImGui::Checkbox("enable", (bool*)DVAR_GET_POINTER(gfx_enable_bloom));
        ImGui::DragFloat("threshold", (float*)DVAR_GET_POINTER(gfx_bloom_threshold), 0.01f, 0.0f, 3.0f);
    });

    CollapseWindow("Path Tracer", []() {
        auto& gm = GraphicsManager::GetSingleton();
        int selected = (int)gm.GetActiveRenderGraphName();
        const int prev_selected = selected;
        for (int i = 0; i < std::to_underlying(RenderGraphName::COUNT); ++i) {
            const char* name = ToString(static_cast<RenderGraphName>(i));
            name = name ? name : "xxx";
            ImGui::RadioButton(name, &selected, i);
        }
        if (prev_selected != selected) {
            if (gm.SetActiveRenderGraph((RenderGraphName)selected)) {
                if (selected == (int)RenderGraphName::PATHTRACER) {
                    gm.StartPathTracer(PathTracerMethod::ACCUMULATIVE);
                }
            }
        }

        if (ImGui::Button("Generate BVH")) {
            DVAR_SET_BOOL(gfx_bvh_generate, true);
        }
        int bvh_level = DVAR_GET_INT(gfx_bvh_debug);
        // @TODO: get rid of hard code
        if (ImGui::DragInt("Debug BVH", &bvh_level, 0.1f, -1, 12)) {
            DVAR_SET_INT(gfx_bvh_debug, bvh_level);
        }
    });
}

}  // namespace my
