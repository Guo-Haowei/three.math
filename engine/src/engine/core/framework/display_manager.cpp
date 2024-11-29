#include "display_manager.h"

#include "engine/core/framework/application.h"
#include "engine/core/framework/common_dvars.h"
#include "engine/drivers/empty/empty_display_manager.h"
#include "engine/drivers/glfw/glfw_display_manager.h"
#if USING(PLATFORM_WINDOWS)
#include "engine/drivers/windows/win32_display_manager.h"
#endif
#include "engine/rendering/graphics_dvars.h"

namespace my {

auto DisplayManager::InitializeImpl() -> Result<void> {
    InitializeKeyMapping();

    const auto& spec = m_app->GetSpecification();

    std::string title{ spec.name };
    switch (spec.backend) {
#define BACKEND_DECLARE(ENUM, STR, ...) \
    case Backend::ENUM:                 \
        title.append(" [" STR "|");     \
        break;
        BACKEND_LIST
#undef BACKEND_DECLARE
        default:
            break;
    }

    title.append(
#if USING(DEBUG_BUILD)
        "Debug]"
#else
        "Release]"
#endif
    );

    WindowSpecfication info = {
        .title = std::move(title),
        .width = spec.width,
        .height = spec.height,
        .backend = spec.backend,
        .decorated = spec.decorated,
        .fullscreen = spec.fullscreen,
        .vsync = spec.vsync,
        .enableImgui = spec.enableImgui,
    };

    return InitializeWindow(info);
}

std::shared_ptr<DisplayManager> DisplayManager::Create() {
    const std::string& backend = DVAR_GET_STRING(gfx_backend);
    // @TODO: cocoa display
    if (backend == "opengl" || backend == "vulkan" || backend == "metal") {
        return std::make_shared<GlfwDisplayManager>();
    }

#if USING(PLATFORM_WINDOWS)
    return std::make_shared<Win32DisplayManager>();
#else
    return std::make_shared<EmptyDisplayManager>();
#endif
}

}  // namespace my
