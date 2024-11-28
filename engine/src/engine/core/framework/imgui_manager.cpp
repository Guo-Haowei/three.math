#include "imgui_manager.h"

#include <imgui/imgui.h>

#include <filesystem>

#include "engine/core/framework/application.h"
#include "engine/core/framework/asset_manager.h"
#include "engine/core/string/string_utils.h"

namespace my {

namespace fs = std::filesystem;

auto ImguiManager::InitializeImpl() -> Result<void> {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    std::string_view engine_folder = StringUtils::BasePath(__FILE__);
    engine_folder = StringUtils::BasePath(engine_folder);
    engine_folder = StringUtils::BasePath(engine_folder);
    engine_folder = StringUtils::BasePath(engine_folder);
    engine_folder = StringUtils::BasePath(engine_folder);

    fs::path font_path{ engine_folder };
    font_path = font_path / "fonts" / "DroidSans.ttf";

    auto res = AssetManager::GetSingleton().LoadFileSync(FilePath(font_path.string()));
    if (!res) {
        return HBN_ERROR(res.error());
    }

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != NULL);

    auto asset = *res;
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(asset->buffer.data(), (int)asset->buffer.size(), 16, &font_cfg);
    if (!font) {
        return HBN_ERROR(ErrorCode::ERR_CANT_CREATE, "Failed to create font '{}'", font_path.string());
    }

    fs::path ini_path = fs::path{ m_app->GetUserFolder() } / "imgui.ini";
    m_imguiSettingsPath = ini_path.string();
    LOG_VERBOSE("imgui settings path is '{}'", m_imguiSettingsPath);
    io.IniFilename = m_imguiSettingsPath.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    auto& style = ImGui::GetStyle();
    auto& colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.105f, 0.11f, 1.0f);

    // Headers
    colors[ImGuiCol_Header] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.3805f, 0.381f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.2805f, 0.281f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    DEV_ASSERT(m_displayInitializeFunc);
    m_displayInitializeFunc();

    DEV_ASSERT(m_rendererInitializeFunc);
    m_rendererInitializeFunc();

    return Result<void>();
}

void ImguiManager::FinalizeImpl() {
    DEV_ASSERT(m_rendererFinalizeFunc);
    m_rendererFinalizeFunc();

    DEV_ASSERT(m_displayFinalizeFunc);
    m_displayFinalizeFunc();

    ImGui::DestroyContext();
}

void ImguiManager::BeginFrame() {
    if (DEV_VERIFY(m_displayBeginFrameFunc)) {
        m_displayBeginFrameFunc();
        ImGui::NewFrame();
    }
}

}  // namespace my
