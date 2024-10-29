#include "render_manager.h"

#include "core/debugger/profiler.h"
#include "core/framework/asset_manager.h"
#include "core/framework/graphics_manager.h"
#include "core/math/geometry.h"
#include "rendering/render_graph/render_graph_defines.h"

#define DEFINE_DVAR
#include "rendering_dvars.h"

namespace {
std::string s_prev_env_map;
bool s_need_update_env = false;
}  // namespace

namespace my::renderer {

bool need_update_env() {
    return s_need_update_env;
}

void reset_need_update_env() {
    s_need_update_env = false;
}

void request_env_map(const std::string& path) {
    if (path == s_prev_env_map) {
        return;
    }

    if (!s_prev_env_map.empty()) {
        LOG_WARN("TODO: release {}", s_prev_env_map.empty());
    }

    s_prev_env_map = path;
    if (auto handle = AssetManager::GetSingleton().findImage(FilePath{ path }); handle) {
        if (auto image = handle->get(); image && image->gpu_texture) {
            g_constantCache.cache.c_hdr_env_map = image->gpu_texture->get_resident_handle();
            g_constantCache.update();
            s_need_update_env = true;
            return;
        }
    }

    AssetManager::GetSingleton().loadImageAsync(FilePath{ path }, [](void* p_asset, void* p_userdata) {
        Image* image = reinterpret_cast<Image*>(p_asset);
        ImageHandle* handle = reinterpret_cast<ImageHandle*>(p_userdata);
        DEV_ASSERT(image);
        DEV_ASSERT(handle);

        handle->set(image);
        GraphicsManager::GetSingleton().RequestTexture(handle, [](Image* p_image) {
            // @TODO: better way
            if (p_image->gpu_texture) {
                g_constantCache.cache.c_hdr_env_map = p_image->gpu_texture->get_resident_handle();
                g_constantCache.update();
                s_need_update_env = true;
            }
        });
    });
}

// @TODO: fix this?

void register_rendering_dvars() {
#define REGISTER_DVAR
#include "rendering_dvars.h"
}

void fill_texture_and_sampler_desc(const Image* p_image, TextureDesc& p_texture_desc, SamplerDesc& p_sampler_desc) {
    DEV_ASSERT(p_image);
    bool is_hdr_file = false;

    switch (p_image->format) {
        case PixelFormat::R32_FLOAT:
        case PixelFormat::R32G32_FLOAT:
        case PixelFormat::R32G32B32_FLOAT:
        case PixelFormat::R32G32B32A32_FLOAT: {
            is_hdr_file = true;
        } break;
        default: {
        } break;
    }

    p_texture_desc.format = p_image->format;
    p_texture_desc.dimension = Dimension::TEXTURE_2D;
    p_texture_desc.width = p_image->width;
    p_texture_desc.height = p_image->height;
    p_texture_desc.array_size = 1;
    p_texture_desc.bind_flags |= BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    p_texture_desc.initial_data = p_image->buffer.data();
    p_texture_desc.mip_levels = 1;

    if (is_hdr_file) {
        p_sampler_desc.min = p_sampler_desc.mag = FilterMode::LINEAR;
        p_sampler_desc.mode_u = p_sampler_desc.mode_v = AddressMode::CLAMP;
    } else {
        p_texture_desc.misc_flags |= RESOURCE_MISC_GENERATE_MIPS;

        p_sampler_desc.min = FilterMode::MIPMAP_LINEAR;
        p_sampler_desc.mag = FilterMode::LINEAR;
    }
}

}  // namespace my::renderer

namespace my {

RenderManager::RenderManager() : Module("RenderManager") {
    for (int i = 0; i < MAX_LIGHT_CAST_SHADOW_COUNT; ++i) {
        m_free_point_light_shadow.push_back(i);
    }
}

bool RenderManager::Initialize() {
    m_screen_quad_buffers = GraphicsManager::GetSingleton().CreateMesh(makePlaneMesh(vec3(1)));
    m_skybox_buffers = GraphicsManager::GetSingleton().CreateMesh(makeSkyBoxMesh());

    return true;
}

void RenderManager::Finalize() {
    m_screen_quad_buffers = nullptr;
    m_skybox_buffers = nullptr;

    m_free_point_light_shadow.clear();
    return;
}

PointShadowHandle RenderManager::allocate_point_light_shadow_map() {
    if (m_free_point_light_shadow.empty()) {
        LOG_WARN("OUT OUT POINT SHADOW MAP");
        return INVALID_POINT_SHADOW_HANDLE;
    }

    int handle = m_free_point_light_shadow.front();
    m_free_point_light_shadow.pop_front();
    return handle;
}

void RenderManager::free_point_light_shadow_map(PointShadowHandle& p_handle) {
    DEV_ASSERT_INDEX(p_handle, MAX_LIGHT_CAST_SHADOW_COUNT);
    m_free_point_light_shadow.push_back(p_handle);
    p_handle = INVALID_POINT_SHADOW_HANDLE;
}

void RenderManager::draw_quad() {
    GraphicsManager::GetSingleton().SetMesh(m_screen_quad_buffers);
    GraphicsManager::GetSingleton().DrawElements(m_screen_quad_buffers->indexCount);
}

void RenderManager::draw_quad_instanced(uint32_t p_instance_count) {
    GraphicsManager::GetSingleton().SetMesh(m_screen_quad_buffers);
    GraphicsManager::GetSingleton().DrawElementsInstanced(p_instance_count, m_screen_quad_buffers->indexCount, 0);
}

void RenderManager::draw_skybox() {
    GraphicsManager::GetSingleton().SetMesh(m_skybox_buffers);
    GraphicsManager::GetSingleton().DrawElements(m_skybox_buffers->indexCount);
}

}  // namespace my
