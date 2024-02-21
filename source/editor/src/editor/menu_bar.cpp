#include "menu_bar.h"

#include "core/framework/common_dvars.h"
#include "core/framework/graphics_manager.h"
#include "core/framework/scene_manager.h"
#include "core/input/input.h"
#include "editor/editor_layer.h"
#include "platform/windows/dialog.h"
#include "rendering/render_graph/render_graph_vxgi.h"
#include "rendering/rendering_dvars.h"

namespace my {

// @TODO: fix this
static std::vector<std::string> quick_dirty_split(std::string str, std::string token) {
    std::vector<std::string> result;
    while (str.size()) {
        size_t index = str.find(token);
        if (index != std::string::npos) {
            result.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.size() == 0) result.push_back(str);
        } else {
            result.push_back(str);
            str = "";
        }
    }
    return result;
}

static void import_scene() {
    std::vector<const char*> filters = { ".gltf", ".obj" };
    auto path = open_file_dialog(filters);

    if (path.empty()) {
        return;
    }

    SceneManager::singleton().request_scene(path);

    std::string files(DVAR_GET_STRING(recent_files));
    if (!files.empty()) {
        files.append(";");
    }
    files.append(path);

    DVAR_SET_STRING(recent_files, files);
}

static void import_recent() {
    std::string recent_files(DVAR_GET_STRING(recent_files));

    auto files = quick_dirty_split(recent_files, ";");

    for (const auto& file : files) {
        if (ImGui::MenuItem(file.c_str())) {
            SceneManager::singleton().request_scene(file);
        }
    }
}

static void save_project(bool open_dialog) {
    const std::string& project = DVAR_GET_STRING(project);

    std::filesystem::path path{ project.empty() ? "untitled.scene" : project.c_str() };
    if (open_dialog || project.empty()) {
        if (!open_save_dialog(path)) {
            return;
        }
    }

    DVAR_SET_STRING(project, path.string());
    Scene& scene = SceneManager::singleton().get_scene();

    Archive archive;
    if (!archive.open_write(path.string())) {
        return;
    }

    if (scene.serialize(archive)) {
        LOG_OK("scene saved to '{}'", path.string());
    }
}

void MenuBar::update(Scene&) {
    if (m_editor.get_displayed_image() == 0) {
        uint32_t handle = GraphicsManager::singleton().find_resource(RT_RES_FXAA)->get_handle();
        m_editor.set_displayed_image(handle);
    }

    // @TODO: input system, key s handled here, don't handle it in viewer
    if (input::is_key_down(KEY_LEFT_CONTROL)) {
        if (input::is_key_pressed(KEY_S)) {
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
        if (ImGui::BeginMenu("Import")) {
            if (ImGui::MenuItem("Import")) {
                import_scene();
            }
            if (ImGui::BeginMenu("Import Recent")) {
                import_recent();
                ImGui::EndMenu();
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
