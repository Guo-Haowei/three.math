#pragma once
#include "engine/core/base/noncopyable.h"
#include "render_pass.h"

namespace my {
class GraphicsManager;
}

namespace my::renderer {

struct DrawData;

class RenderGraph : public NonCopyable {
public:
    struct Edge {
        int from;
        int to;
    };

    std::shared_ptr<RenderPass> CreatePass(RenderPassDesc& p_desc);
    std::shared_ptr<RenderPass> FindPass(RenderPassName p_name) const;

    // @TODO: graph should not own resource, move to graphics manager

    void Compile();

    void Execute(const renderer::DrawData& p_data, GraphicsManager& p_graphics_manager);

private:
    std::vector<std::shared_ptr<RenderPass>> m_renderPasses;
    std::vector<int> m_sortedOrder;
    std::vector<Edge> m_links;
    std::vector<std::vector<int>> m_levels;

    std::map<RenderPassName, int> m_renderPassLookup;

    friend class RenderGraphViewer;
};

}  // namespace my::renderer
