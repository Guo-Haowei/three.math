#pragma once
#include "editor/editor_window.h"
#include "engine/core/base/graph.h"

namespace my::renderer {
class RenderPass;
}

namespace my {

class RenderGraphViewer : public EditorWindow {
public:
    RenderGraphViewer(EditorLayer& editor) : EditorWindow("RenderGraph", editor) {}

protected:
    void UpdateInternal(Scene& scene) override;
    void DrawNodes(bool p_first_frame, const Graph<renderer::RenderPass*> p_graph);
};

}  // namespace my