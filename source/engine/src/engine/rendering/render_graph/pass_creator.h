#pragma once
#include "rendering/gpu_resource.h"
#include "rendering/pixel_format.h"
#include "rendering/render_graph/render_graph_defines.h"

// @TODO: refactor this
namespace my::rg {

class RenderGraph;

class RenderPassCreator {
public:
    struct Config {
        bool enable_shadow = true;
        bool enable_point_shadow = true;
        bool enable_voxel_gi = true;
        bool enable_ibl = true;
        bool enable_bloom = true;

        int frame_width;
        int frame_height;
    };

    RenderPassCreator(const Config& p_config, RenderGraph& p_graph)
        : m_config(p_config),
          m_graph(p_graph) {}

    static void CreateDummy(RenderGraph& p_graph);
    static void CreateDefault(RenderGraph& p_graph);
    static void CreateVxgi(RenderGraph& p_graph);
    // @TODO: add this
    static void CreatePathTracer(RenderGraph& p_graph);

private:
    void AddGBufferPass();
    void AddShadowPass();
    void AddLightingPass();
    void AddEmitterPass();
    void AddBloomPass();
    void AddTonePass();

    static GpuTextureDesc BuildDefaultTextureDesc(RenderTargetResourceName p_name,
                                                  PixelFormat p_format,
                                                  AttachmentType p_type,
                                                  uint32_t p_width,
                                                  uint32_t p_height);

    Config m_config;
    RenderGraph& m_graph;
};

}  // namespace my::rg
