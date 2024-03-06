#pragma once
#include "rendering/pixel_format.h"

namespace my {

enum Dimension : uint32_t {
    TEXTURE_2D,
    TEXTURE_3D,
    TEXTURE_2D_ARRAY,
    TEXTURE_CUBE,
};

// @TODO: enum class

enum BindFlags : uint32_t {
    BIND_NONE = BIT(0),
    BIND_VERTEX_BUFFER = BIT(1),
    BIND_INDEX_BUFFER = BIT(2),
    BIND_CONSTANT_BUFFER = BIT(3),
    BIND_SHADER_RESOURCE = BIT(4),
    BIND_STREAM_OUTPUT = BIT(5),
    BIND_RENDER_TARGET = BIT(6),
    BIND_DEPTH_STENCIL = BIT(7),
    BIND_UNORDERED_ACCESS = BIT(8),
    BIND_DECODER = BIT(9),
    BIND_VIDEO_ENCODER = BIT(10),
};

enum ResourceMiscFlags : uint32_t {
    RESOURCE_MISC_NONE = BIT(0),
    RESOURCE_MISC_GENERATE_MIPS = BIT(1),
    RESOURCE_MISC_SHARED = BIT(2),
    RESOURCE_MISC_TEXTURECUBE = BIT(3),
    RESOURCE_MISC_DRAWINDIRECT_ARGS = BIT(4),
    RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS = BIT(5),
    RESOURCE_MISC_BUFFER_STRUCTURED = BIT(6),
    RESOURCE_MISC_RESOURCE_CLAMP = BIT(7),
    RESOURCE_MISC_SHARED_KEYEDMUTEX = BIT(8),
    RESOURCE_MISC_GDI_COMPATIBLE = BIT(9),
    RESOURCE_MISC_SHARED_NTHANDLE = BIT(10),
    RESOURCE_MISC_RESTRICTED_CONTENT = BIT(11),
    RESOURCE_MISC_RESTRICT_SHARED_RESOURCE = BIT(12),
    RESOURCE_MISC_RESTRICT_SHARED_RESOURCE_DRIVER = BIT(13),
    RESOURCE_MISC_GUARDED = BIT(14),
    RESOURCE_MISC_TILE_POOL = BIT(15),
    RESOURCE_MISC_TILED = BIT(16),
    RESOURCE_MISC_HW_PROTECTED = BIT(17),
    RESOURCE_MISC_SHARED_DISPLAYABLE = BIT(18),
    RESOURCE_MISC_SHARED_EXCLUSIVE_WRITER = BIT(19),
};

struct TextureDesc {
    Dimension dimension;
    uint32_t width;
    uint32_t height;
    uint32_t mip_levels;
    uint32_t array_size;
    PixelFormat format;
    uint32_t bind_flags;
    uint32_t misc_flags;
    const void* initial_data;
};

struct Texture {
    Texture(const TextureDesc& p_desc) : desc(p_desc) {}

    virtual ~Texture() = default;

    virtual uint64_t get_resident_handle() const = 0;
    virtual uint64_t get_handle() const = 0;

    uint32_t get_handle32() const { return static_cast<uint32_t>(get_handle()); }

    const TextureDesc desc;
};

}  // namespace my
