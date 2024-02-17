#pragma once
#include "assets/image.h"

namespace my::rg {

enum ResourceType {
    RT_COLOR_ATTACHMENT,
    RT_DEPTH_ATTACHMENT,
    RT_SHADOW_MAP,
    RT_SHADDW_MAP_ARRAY,
    RT_SHADDW_MAP_CUBE,
};

struct ResourceDesc {
    std::string name;
    PixelFormat format;
    ResourceType type;
    int width;
    int height;

    ResourceDesc(const char* p_name, PixelFormat p_format, ResourceType p_type, int p_width, int p_height)
        : name(p_name), format(p_format), type(p_type), width(p_width), height(p_height) {
    }
};

class Resource {
public:
    Resource(const ResourceDesc& p_desc) : m_desc(p_desc) {}

    const ResourceDesc get_desc() const { return m_desc; }
    uint32_t get_handle() const { return m_handle; }

private:
    const ResourceDesc m_desc;
    uint32_t m_handle = 0;

    friend class RenderGraph;
};

}  // namespace my::rg
