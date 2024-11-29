#include "metal_graphics_manager.h"
#include <imgui/backends/imgui_impl_metal.h>

#include "engine/core/framework/application.h"
#include "engine/core/framework/imgui_manager.h"
#include "engine/drivers/empty/empty_pipeline_state_manager.h"
#include "engine/drivers/glfw/glfw_display_manager.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

namespace my {

MetalGraphicsManager::MetalGraphicsManager() : EmptyGraphicsManager("MetalGraphicsManager", Backend::METAL, NUM_FRAMES_IN_FLIGHT) {
    m_pipelineStateManager = std::make_shared<EmptyPipelineStateManager>();
}

static GLFWwindow* window;
id <MTLDevice> device;
id <MTLCommandQueue> commandQueue;
MTLRenderPassDescriptor *renderPassDescriptor;
CAMetalLayer *layer;

auto MetalGraphicsManager::InitializeInternal() -> Result<void> {
    @autoreleasepool
    {
    auto display_manager = dynamic_cast<GlfwDisplayManager*>(m_app->GetDisplayServer());
    DEV_ASSERT(display_manager);
    if (!display_manager) {
        return HBN_ERROR(ErrorCode::ERR_INVALID_DATA, "display manager is nullptr");
    }
    
    window = display_manager->GetGlfwWindow();
    
    device = MTLCreateSystemDefaultDevice();
    commandQueue = [device newCommandQueue];

    NSWindow *nswin = glfwGetCocoaWindow(window);
    layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswin.contentView.layer = layer;
    nswin.contentView.wantsLayer = YES;

    auto imgui = m_app->GetImguiManager();
    if (imgui) {
        imgui->SetRenderCallbacks(
            []() {
                ImGui_ImplMetal_Init(device);
                renderPassDescriptor = [MTLRenderPassDescriptor new];
                ImGui_ImplMetal_NewFrame(renderPassDescriptor);
            },
            []() {
                ImGui_ImplMetal_Shutdown();
            });
        }
    }

    return Result<void>();
}

void MetalGraphicsManager::FinalizeImpl() {
}

void MetalGraphicsManager::Present() {
    @autoreleasepool
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        layer.drawableSize = CGSizeMake(width, height);
        id<CAMetalDrawable> drawable = [layer nextDrawable];

        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        float clear_color[4] = {0.3f, 0.4f, 0.3f, 1.0f};
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(clear_color[0] * clear_color[3], clear_color[1] * clear_color[3], clear_color[2] * clear_color[3], clear_color[3]);
        renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder pushDebugGroup:@"ImGui demo"];

        // Start the Dear ImGui frame
        ImGui_ImplMetal_NewFrame(renderPassDescriptor);

        ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

        ImGuiIO& io = ImGui::GetIO();
        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        [renderEncoder popDebugGroup];
        [renderEncoder endEncoding];

        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];
    }
}

void MetalGraphicsManager::OnWindowResize(int p_width, int p_height) {
    unused(p_width);
    unused(p_height);
}

}
