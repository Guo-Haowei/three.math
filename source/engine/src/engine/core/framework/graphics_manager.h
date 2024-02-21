#pragma once
#include "assets/image.h"
#include "core/base/singleton.h"
#include "core/framework/event_queue.h"
#include "core/framework/module.h"
#include "rendering/render_graph/render_graph.h"
#include "scene/material_component.h"
#include "scene/scene_components.h"

// @TODO: refactor
struct MaterialConstantBuffer;

namespace my {

struct RenderData;

class GraphicsManager : public Singleton<GraphicsManager>, public Module, public EventListener {
public:
    enum RenderGraph {
        RENDER_GRAPH_NONE,
        RENDER_GRAPH_BASE_COLOR,
        RENDER_GRAPH_VXGI,
    };

    GraphicsManager(std::string_view p_name) : Module(p_name) {}

    virtual void render() = 0;

    // @TODO: thread safety ?
    void event_received(std::shared_ptr<Event> p_event) final;

    // @TODO: filter
    virtual void create_texture(ImageHandle* p_handle) = 0;

    virtual std::shared_ptr<RenderTarget> create_resource(const RenderTargetDesc& p_desc) = 0;

    virtual uint32_t get_final_image() const = 0;

    virtual std::shared_ptr<RenderTarget> find_resource(const std::string& p_name) const = 0;

    // @TODO: refactor this
    virtual void fill_material_constant_buffer(const MaterialComponent* p_material, MaterialConstantBuffer& p_cb) = 0;

    std::shared_ptr<RenderData> get_render_data() { return m_render_data; }
    const rg::RenderGraph& get_active_render_graph() { return m_render_graph; }

    static std::shared_ptr<GraphicsManager> create();

protected:
    virtual void on_scene_change(const Scene& p_scene) = 0;

    RenderGraph m_method = RENDER_GRAPH_NONE;

    std::shared_ptr<RenderData> m_render_data;

    rg::RenderGraph m_render_graph;

    std::map<std::string, std::shared_ptr<RenderTarget>> m_resource_lookup;
};

}  // namespace my
