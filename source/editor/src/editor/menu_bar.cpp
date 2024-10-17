#include "menu_bar.h"

#include "core/framework/common_dvars.h"
#include "core/framework/graphics_manager.h"
#include "core/framework/scene_manager.h"
#include "core/input/input.h"
#include "drivers/windows/dialog.h"
#include "editor/editor_layer.h"
#include "rendering/rendering_dvars.h"

namespace my {

static void save_project(bool open_dialog) {
    const std::string& project = DVAR_GET_STRING(project);

    std::filesystem::path path{ project.empty() ? "untitled.scene" : project.c_str() };
    if (open_dialog || project.empty()) {
        if (!open_save_dialog(path)) {
            return;
        }
    }

    DVAR_SET_STRING(project, path.string());
    Scene& scene = SceneManager::singleton().getScene();

    Archive archive;
    if (!archive.openWrite(path.string())) {
        return;
    }

    if (scene.serialize(archive)) {
        LOG_OK("scene saved to '{}'", path.string());
    }
}

void MenuBar::update(Scene&) {
    // @TODO: input system, key s handled here, don't handle it in viewer
    if (input::isKeyDown(KEY_LEFT_CONTROL)) {
        if (input::isKeyPressed(KEY_S)) {
            save_project(false);
        }
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                save_project(false);
            }
            if (ImGui::MenuItem("Save As..")) {
                save_project(true);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {
            }
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {
            }  // Disabled item
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "CTRL+X")) {
            }
            if (ImGui::MenuItem("copy", "CTRL+C")) {
            }
            if (ImGui::MenuItem("Paste", "CTRL+V")) {
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        EditorItem::open_add_entity_popup(ecs::Entity::INVALID);
        ImGui::EndMenuBar();
    }
}

}  // namespace my
