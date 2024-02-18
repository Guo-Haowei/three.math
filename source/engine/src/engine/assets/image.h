#pragma once
#include "assets/asset_handle.h"
#include "rendering/pixel_format.h"

namespace my {

// @TODO: refactor
struct Texture {
    uint32_t handle;
    uint64_t resident_handle;
};

class Image {
public:
    Image(PixelFormat p_format, int p_width, int p_height, int p_num_channels, std::vector<uint8_t>& p_buffer)
        : format(p_format),
          width(p_width),
          height(p_height),
          num_channels(p_num_channels),
          buffer(std::move(p_buffer)) {
    }

    PixelFormat format;
    const int width;
    const int height;
    const int num_channels;
    std::vector<uint8_t> buffer;

    Texture texture;
};

}  // namespace my
