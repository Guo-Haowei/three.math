#pragma once
#include "editor/editor_window.h"
#include "scene/camera_controller.h"

namespace my {

class Viewer : public EditorWindow {
public:
    Viewer(EditorLayer& p_editor) : EditorWindow("Viewer", p_editor) {}

protected:
    void UpdateInternal(Scene& p_scene) override;

private:
    void UpdateData();
    void SelectEntity(Scene& p_scene, const Camera& p_camera);
    void DrawGui(Scene& p_scene, Camera& p_camera);

    vec2 m_canvasMin;
    vec2 m_canvasSize;
    bool m_focused;
    // @TODO: move camera controller to somewhere else
    CameraController m_cameraController;
};

}  // namespace my
