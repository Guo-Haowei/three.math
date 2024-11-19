#include "display_manager.h"

#include "core/framework/common_dvars.h"
#include "core/framework/graphics_manager.h"
#include "drivers/empty/empty_display_manager.h"
#include "drivers/glfw/glfw_display_manager.h"
#if USING(PLATFORM_APPLE)
#include "drivers/apple/cocoa_display_manager.h"
#endif
#if USING(PLATFORM_WINDOWS)
#include "drivers/windows/win32_display_manager.h"
#endif
#include "rendering/graphics_dvars.h"

namespace my {

bool DisplayManager::Initialize() {
    InitializeKeyMapping();

    const ivec2 resolution = DVAR_GET_IVEC2(window_resolution);
    const ivec2 min_size = ivec2(600, 400);
    const ivec2 size = glm::max(min_size, resolution);

    // Implement GetScreenSize
#if 0
    const GLFWvidmode* vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    const ivec2 size = glm::clamp(resolution, min_size, max_size);
#endif

    CreateInfo info = {
        .width = size.x,
        .height = size.y,
        .title = "Editor"
    };

    auto backend = GraphicsManager::GetSingleton().GetBackend();
    switch (backend) {
        case my::Backend::OPENGL:
            // @TODO: configure version
            info.title.append(" (OpenGL 4.6)");
            break;
        case my::Backend::D3D11:
            info.title.append(" (Direct3D 11)");
            break;
        case my::Backend::D3D12:
            info.title.append(" (Direct3D 12)");
            break;
        case my::Backend::METAL:
            info.title.append(" (Metal)");
            break;
        default:
            break;
    }

    return InitializeWindow(info);
}

std::shared_ptr<DisplayManager> DisplayManager::Create() {
    const std::string& backend = DVAR_GET_STRING(gfx_backend);
    if (backend == "opengl") {
        return std::make_shared<GlfwDisplayManager>();
    }
#if USING(PLATFORM_WINDOWS)
    return std::make_shared<Win32DisplayManager>();
#elif USING(PLATFORM_APPLE)
    return std::make_shared<CocoaDisplayManager>();
#else
    return std::make_shared<EmptyDisplayManager>();
#endif
}

}  // namespace my
