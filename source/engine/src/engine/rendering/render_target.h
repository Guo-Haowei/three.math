#pragma once
#include "rendering/pixel_format.h"
#include "rendering/texture.h"

namespace my {

enum class AttachmentType {
    COLOR_2D,
    COLOR_CUBE_MAP,
    DEPTH_2D,
    DEPTH_STENCIL_2D,
    SHADOW_2D,
    SHADOW_CUBE_MAP,
};

struct RenderTargetDesc {
    std::string name;
    PixelFormat format;
    AttachmentType type;
    int width;
    int height;
    bool gen_mipmap;

    RenderTargetDesc(const std::string& p_name, PixelFormat p_format, AttachmentType p_type, int p_width, int p_height, bool p_gen_mipmap = false)
        : name(p_name), format(p_format), type(p_type), width(p_width), height(p_height), gen_mipmap(p_gen_mipmap) {
    }
};

struct RenderTarget {
    RenderTarget(const RenderTargetDesc& p_desc) : desc(p_desc) {}

    std::tuple<int, int> get_size() const { return std::make_tuple(desc.width, desc.height); }
    const RenderTargetDesc desc;
    std::shared_ptr<Texture> texture;
};

}  // namespace my
