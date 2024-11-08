#pragma once
#include "core/framework/display_manager.h"
#include "core/input/input_code.h"

struct GLFWwindow;

namespace my {

class GlfwDisplayManager : public DisplayManager {
public:
    void Finalize() final;

    bool ShouldClose() final;

    std::tuple<int, int> GetWindowSize() final;
    std::tuple<int, int> GetWindowPos() final;

    void NewFrame() final;
    void Present() final;

private:
    bool InitializeWindow() final;
    void InitializeKeyMapping() final;

    static void cursor_pos_callback(GLFWwindow* window, double x, double y);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void key_callback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    GLFWwindow* m_window = nullptr;

    // @TODO: make it non-static
    inline static std::unordered_map<int, KeyCode> s_key_mapping;
};

}  // namespace my
