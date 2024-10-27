#include "application.h"

#include "core/debugger/profiler.h"
#include "core/dynamic_variable/dynamic_variable_manager.h"
#include "core/framework/asset_manager.h"
#include "core/framework/common_dvars.h"
#include "core/framework/display_manager.h"
#include "core/framework/graphics_manager.h"
#include "core/framework/imgui_module.h"
#include "core/framework/physics_manager.h"
#include "core/framework/scene_manager.h"
#include "core/input/input.h"
#include "core/os/threads.h"
#include "core/os/timer.h"
#include "core/systems/job_system.h"
#include "imgui/imgui.h"
#include "rendering/render_manager.h"
#include "rendering/rendering_dvars.h"

#define DEFINE_DVAR
#include "core/framework/common_dvars.h"

namespace my {

static void registerCommonDvars() {
#define REGISTER_DVAR
#include "core/framework/common_dvars.h"
}

void Application::addLayer(std::shared_ptr<Layer> layer) {
    m_layers.emplace_back(layer);
}

void Application::saveCommandLine(int argc, const char** argv) {
    m_app_name = argv[0];
    for (int i = 1; i < argc; ++i) {
        m_command_line.push_back(argv[i]);
    }
}

void Application::registerModule(Module* module) {
    module->m_app = this;
    m_modules.push_back(module);
}

void Application::setupModules() {
    m_asset_manager = std::make_shared<AssetManager>();
    m_scene_manager = std::make_shared<SceneManager>();
    m_physics_manager = std::make_shared<PhysicsManager>();
    m_imgui_module = std::make_shared<ImGuiModule>();
    m_display_server = DisplayManager::create();
    m_graphics_manager = GraphicsManager::create();
    m_render_manager = std::make_shared<RenderManager>();

    registerModule(m_asset_manager.get());
    registerModule(m_scene_manager.get());
    registerModule(m_physics_manager.get());
    registerModule(m_imgui_module.get());
    registerModule(m_display_server.get());
    registerModule(m_graphics_manager.get());
    registerModule(m_render_manager.get());

    m_event_queue.registerListener(m_graphics_manager.get());
    m_event_queue.registerListener(m_physics_manager.get());
}

int Application::run(int argc, const char** argv) {
    saveCommandLine(argc, argv);
    m_os = std::make_shared<OS>();

    // intialize
    OS::singleton().initialize();

    // dvars
    registerCommonDvars();
    renderer::register_rendering_dvars();
    DynamicVariableManager::deserialize();
    DynamicVariableManager::parse(m_command_line);

    setupModules();

    thread::initialize();
    jobsystem::Initialize();

    for (Module* module : m_modules) {
        LOG("module '{}' being initialized...", module->getName());
        if (!module->initialize()) {
            LOG_ERROR("Error: failed to initialize module '{}'", module->getName());
            return 1;
        }
        LOG("module '{}' initialized\n", module->getName());
    }

    initLayers();
    for (auto& layer : m_layers) {
        layer->attach();
        LOG("[Runtime] layer '{}' attached!", layer->getName());
    }

    LOG("\n********************************************************************************"
        "\nMain Loop"
        "\n********************************************************************************");

    LOG_WARN("TODO: properly unload scene");
    LOG_WARN("TODO: make camera a component");
    LOG_WARN("TODO: refactor render graph");

    LOG_ERROR("TODO: path tracer here");
    LOG_ERROR("TODO: cloth physics");
    LOG_ERROR("TODO: reverse z");

    LOG_OK("TODO: particles");

    // @TODO: add frame count, elapsed time, etc
    Timer timer;
    while (!DisplayManager::singleton().shouldClose()) {
        OPTICK_FRAME("MainThread");

        m_display_server->newFrame();

        input::beginFrame();

        // @TODO: better elapsed time
        float dt = static_cast<float>(timer.getDuration().toSecond());
        dt = glm::min(dt, 0.5f);
        timer.start();

        // to avoid empty renderer crash
        ImGui::NewFrame();
        for (auto& layer : m_layers) {
            layer->update(dt);
        }

        for (auto& layer : m_layers) {
            layer->render();
        }
        ImGui::Render();

        m_scene_manager->update(dt);
        auto& scene = m_scene_manager->getScene();

        m_physics_manager->update(scene);
        m_graphics_manager->update(scene);

        renderer::reset_need_update_env();

        m_display_server->present();

        input::endFrame();
    }

    LOG("\n********************************************************************************"
        "\nMain Loop"
        "\n********************************************************************************");

    // @TODO: fix
    auto [w, h] = DisplayManager::singleton().getWindowSize();
    DVAR_SET_IVEC2(window_resolution, w, h);

    m_layers.clear();

    // @TODO: move it to request shutdown
    thread::requestShutdown();

    // finalize

    for (int index = (int)m_modules.size() - 1; index >= 0; --index) {
        Module* module = m_modules[index];
        module->finalize();
        LOG_VERBOSE("module '{}' finalized", module->getName());
    }

    jobsystem::Finalize();
    thread::finailize();

    DynamicVariableManager::serialize();

    OS::singleton().finalize();

    return 0;
}

}  // namespace my