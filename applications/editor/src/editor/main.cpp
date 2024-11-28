#include "editor/editor_layer.h"
#include "engine/core/framework/entry_point.h"
#include "engine/core/string/string_utils.h"

namespace my {

class Editor : public Application {
public:
    using Application::Application;

    void InitLayers() override {
        AddLayer(std::make_shared<my::EditorLayer>());
    }
};

Application* CreateApplication() {
    std::string_view root = StringUtils::BasePath(__FILE__);
    root = StringUtils::BasePath(root);
    root = StringUtils::BasePath(root);

    ApplicationSpec spec{};
    spec.rootDirectory = root;
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

}  // namespace my

int main(int p_argc, const char** p_argv) {
    return my::Main(p_argc, p_argv);
}
