#include <filesystem>

#include "editor/editor_layer.h"
#include "engine/core/framework/entry_point.h"
#include "engine/core/string/string_utils.h"

#define DEFINE_DVAR
#include "editor_dvars.h"
#undef DEFINE_DVAR

namespace my {

namespace fs = std::filesystem;

extern Scene* CreateTheAviatorScene();
extern Scene* CreateBoxScene();
extern Scene* CreatePbrTestScene();
extern Scene* CreatePhysicsTestScene();

class Editor : public Application {
public:
    Editor(const ApplicationSpec& p_spec) : Application(p_spec, Application::Type::EDITOR) {}

    void InitLayers() override {
        m_editorLayer = std::make_unique<EditorLayer>();
        AttachLayer(m_editorLayer.get());

        // Only creates game layer, don't attach yet
        m_gameLayer = std::make_unique<GameLayer>("GameLayer");
    }

    Scene* CreateInitialScene() override;

private:
    void RegisterDvars() override;

    std::unique_ptr<EditorLayer> m_editorLayer;
};

void Editor::RegisterDvars() {
    Application::RegisterDvars();
#define REGISTER_DVAR
#include "editor_dvars.h"
#undef REGISTER_DVAR
}

Application* CreateApplication() {
    std::string_view root = StringUtils::BasePath(__FILE__);
    root = StringUtils::BasePath(root);
    root = StringUtils::BasePath(root);

    auto res_path = fs::path{ root } / "resources";
    auto res_string = res_path.string();
    auto user_path = fs::path{ root } / "user";
    auto user_string = user_path.string();

    ApplicationSpec spec{};
    spec.resourceFolder = res_string;
    spec.userFolder = user_string;
    spec.name = "Editor";
    spec.width = 800;
    spec.height = 600;
    spec.backend = Backend::EMPTY;
    spec.decorated = true;
    spec.fullscreen = false;
    spec.vsync = false;
    spec.enableImgui = true;
    return new Editor(spec);
}

Scene* Editor::CreateInitialScene() {
    auto scene = DVAR_GET_STRING(default_scene);
    if (scene == "pbr_test") {
        return CreatePbrTestScene();
    }
    if (scene == "physics_test") {
        return CreatePhysicsTestScene();
    }
    if (scene == "the_aviator") {
        return CreateTheAviatorScene();
    }
    if (scene == "box") {
        return CreateBoxScene();
    }

    return Application::CreateInitialScene();
}

}  // namespace my

int main(int p_argc, const char** p_argv) {
    return my::Main(p_argc, p_argv);
}
