#pragma once
#include "rendering/render_graph/render_graph_defines.h"

namespace my::rg {

class RenderPassCreator {
public:
    struct Config {
        bool enable_shadow = true;
        bool enable_ssao = true;
        bool enable_voxel_gi = true;
        bool enable_ibl = true;
        bool enable_bloom = true;

        int frame_width;
        int frame_height;
    };

    RenderPassCreator(const Config& p_config, RenderGraph& p_graph)
        : m_config(p_config),
          m_graph(p_graph) {}

    void add_gbuffer_pass();
    void add_shadow_pass();
    void add_lighting_pass();
    void add_bloom_pass();

private:
    Config m_config;
    RenderGraph& m_graph;
};

}  // namespace my::rg
