#pragma once
#include "assets/image.h"
#include "core/base/singleton.h"
#include "core/framework/module.h"
#include "rendering/sampler.h"
#include "rendering/texture.h"
#include "scene/scene.h"

// @TODO: remove this class
#include "core/framework/graphics_manager.h"

namespace my {

using PointShadowHandle = int;
constexpr PointShadowHandle INVALID_POINT_SHADOW_HANDLE = -1;

class RenderManager : public Singleton<RenderManager>, public Module {
public:
    RenderManager();

    bool initialize() override;
    void finalize() override;

    void update(Scene&) {}

    PointShadowHandle allocate_point_light_shadow_map();
    void free_point_light_shadow_map(PointShadowHandle& p_handle);

    void draw_quad();
    void draw_skybox();

private:
    std::list<PointShadowHandle> m_free_point_light_shadow;
    const MeshBuffers* m_screen_quad_buffers;
    const MeshBuffers* m_skybox_buffers;
};

}  // namespace my

namespace my::renderer {

bool need_update_env();
void reset_need_update_env();
void request_env_map(const std::string& path);

void register_rendering_dvars();

void fill_texture_and_sampler_desc(const Image* p_image, TextureDesc& p_texture_desc, SamplerDesc& p_sampler_desc);

}  // namespace my::renderer
