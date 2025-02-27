#pragma once

namespace my {

enum class PixelFormat {
    UNKNOWN,

    R8_UINT,
    R8G8_UINT,
    R8G8B8_UINT,
    R8G8B8A8_UINT,

    R8G8B8A8_UNORM,

    R16_FLOAT,
    R16G16_FLOAT,
    R16G16B16_FLOAT,
    R16G16B16A16_FLOAT,

    R32_FLOAT,
    R32G32_FLOAT,
    R32G32B32_FLOAT,
    R32G32B32A32_FLOAT,

    R32G32_SINT,
    R32G32B32_SINT,
    R32G32B32A32_SINT,

    R11G11B10_FLOAT,
    R10G10B10A2_UINT,

    D32_FLOAT,

    R24G8_TYPELESS,
    R24_UNORM_X8_TYPELESS,
    D24_UNORM_S8_UINT,
    X24_TYPELESS_G8_UINT,

    R32G8X24_TYPELESS,
    D32_FLOAT_S8X24_UINT,

    COUNT,
};

// @TODO: refactor
inline uint32_t channel_size(PixelFormat p_format) {
    switch (p_format) {
        case PixelFormat::R8_UINT:
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8A8_UINT:
            return sizeof(uint8_t);
        case PixelFormat::R16_FLOAT:
        case PixelFormat::R16G16_FLOAT:
        case PixelFormat::R16G16B16_FLOAT:
        case PixelFormat::R16G16B16A16_FLOAT:
            return sizeof(uint16_t);
        case PixelFormat::R32_FLOAT:
        case PixelFormat::R32G32_FLOAT:
        case PixelFormat::R32G32B32_FLOAT:
        case PixelFormat::R32G32B32A32_FLOAT:
        case PixelFormat::D32_FLOAT:
            return sizeof(float);
        default:
            CRASH_NOW();
            return 0;
    }
}

inline uint32_t channel_count(PixelFormat p_format) {
    switch (p_format) {
        case PixelFormat::R8_UINT:
        case PixelFormat::R16_FLOAT:
        case PixelFormat::R32_FLOAT:
        case PixelFormat::D32_FLOAT:
            return 1;
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R16G16_FLOAT:
        case PixelFormat::R32G32_FLOAT:
            return 2;
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R16G16B16_FLOAT:
        case PixelFormat::R32G32B32_FLOAT:
            return 3;
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R16G16B16A16_FLOAT:
        case PixelFormat::R32G32B32A32_FLOAT:
            return 4;
        default:
            CRASH_NOW();
            return 0;
    }
}

}  // namespace my
