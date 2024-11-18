#ifdef CONVERT_H_INCLUDED
#error DO NOT INCLUDE THIS IN A HEADER FILE
#endif
#define CONVERT_H_INCLUDED

#if !defined(INCLUDE_AS_D3D11) && !defined(INCLUDE_AS_D3D12)
#define INCLUDE_AS_D3D11
#endif  // !INCLUDE_AS_D3D11) && !defined(INCLUDE_AS_D3D12)

// @TODO: move to a common place
#if defined(INCLUDE_AS_D3D11)
#include <d3d11.h>
#define D3D_(a)                      D3D11_##a
#define D3D_INPUT_CLASSIFICATION_(a) D3D11_INPUT_##a
#define D3D_FILL_MODE_(a)            D3D11_FILL_##a
#define D3D_CULL_MODE_(a)            D3D11_CULL_##a
#define D3D_COMPARISON_(a)           D3D11_COMPARISON_##a
#define D3D_FILTER_(a)               D3D11_FILTER_##a
#define D3D_TEXTURE_ADDRESS_MODE_(a) D3D11_TEXTURE_ADDRESS_##a
#elif defined(INCLUDE_AS_D3D12)
#include <d3d12.h>
#define D3D_(a)                      D3D12_##a
#define D3D_INPUT_CLASSIFICATION_(a) D3D12_INPUT_CLASSIFICATION_##a
#define D3D_FILL_MODE_(a)            D3D12_FILL_MODE_##a
#define D3D_CULL_MODE_(a)            D3D12_CULL_MODE_##a
#define D3D_COMPARISON_(a)           D3D12_COMPARISON_FUNC_##a
#define D3D_FILTER_(a)               D3D12_FILTER_##a
#define D3D_TEXTURE_ADDRESS_MODE_(a) D3D12_TEXTURE_ADDRESS_MODE_##a
#else
#error "Unknown API"
#endif

#include "rendering/pipeline_state.h"
#include "rendering/pixel_format.h"

namespace my::d3d {

using D3D_INPUT_CLASSIFICATION = D3D_(INPUT_CLASSIFICATION);
using D3D_FILL_MODE = D3D_(FILL_MODE);
using D3D_CULL_MODE = D3D_(CULL_MODE);
using D3D_DEPTH_WRITE_MASK = D3D_(DEPTH_WRITE_MASK);
using D3D_COMPARISON_FUNC = D3D_(COMPARISON_FUNC);
using D3D_FILTER = D3D_(FILTER);
using D3D_TEXTURE_ADDRESS_MODE = D3D_(TEXTURE_ADDRESS_MODE);

static inline DXGI_FORMAT Convert(PixelFormat p_format) {
    switch (p_format) {
        // @TODO: use macro
        case PixelFormat::UNKNOWN:
            return DXGI_FORMAT_UNKNOWN;
        case PixelFormat::R8_UINT:
            return DXGI_FORMAT_R8_UNORM;
        case PixelFormat::R8G8_UINT:
            return DXGI_FORMAT_R8G8_UNORM;
        case PixelFormat::R8G8B8_UINT:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R8G8B8A8_UINT:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R16_FLOAT:
            return DXGI_FORMAT_R16_FLOAT;
        case PixelFormat::R16G16_FLOAT:
            return DXGI_FORMAT_R16G16_FLOAT;
        case PixelFormat::R16G16B16_FLOAT:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case PixelFormat::R16G16B16A16_FLOAT:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case PixelFormat::R32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case PixelFormat::R32G32_FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case PixelFormat::R32G32B32_FLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case PixelFormat::R32G32B32A32_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case PixelFormat::R32G32_SINT:
            return DXGI_FORMAT_R32G32_SINT;
        case PixelFormat::R32G32B32_SINT:
            return DXGI_FORMAT_R32G32B32_SINT;
        case PixelFormat::R32G32B32A32_SINT:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case PixelFormat::R11G11B10_FLOAT:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case PixelFormat::D32_FLOAT:
            return DXGI_FORMAT_D32_FLOAT;
        case PixelFormat::R24G8_TYPELESS:
            return DXGI_FORMAT_R24G8_TYPELESS;
        case PixelFormat::R24_UNORM_X8_TYPELESS:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case PixelFormat::D24_UNORM_S8_UINT:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case PixelFormat::X24_TYPELESS_G8_UINT:
            return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        case PixelFormat::R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        default:
            CRASH_NOW();
            return DXGI_FORMAT_UNKNOWN;
    }
}

static inline D3D_INPUT_CLASSIFICATION Convert(InputClassification p_input_classification) {
    switch (p_input_classification) {
        case InputClassification::PER_VERTEX_DATA:
            return D3D_INPUT_CLASSIFICATION_(PER_VERTEX_DATA);
        case InputClassification::PER_INSTANCE_DATA:
            return D3D_INPUT_CLASSIFICATION_(PER_INSTANCE_DATA);
    }
    return D3D_INPUT_CLASSIFICATION_(PER_VERTEX_DATA);
}

static inline D3D_FILL_MODE Convert(FillMode p_fill_mode) {
    switch (p_fill_mode) {
        case FillMode::SOLID:
            return D3D_FILL_MODE_(SOLID);
        case FillMode::WIREFRAME:
            return D3D_FILL_MODE_(WIREFRAME);
    }
    return D3D_FILL_MODE_(SOLID);
}

static inline D3D_CULL_MODE Convert(CullMode p_cull_mode) {
    switch (p_cull_mode) {
        case CullMode::NONE:
            return D3D_CULL_MODE_(NONE);
        case CullMode::FRONT:
            return D3D_CULL_MODE_(FRONT);
        case CullMode::BACK:
            return D3D_CULL_MODE_(BACK);
    }
    return D3D_CULL_MODE_(NONE);
}

static inline D3D_COMPARISON_FUNC Convert(ComparisonFunc p_func) {
    switch (p_func) {
        case ComparisonFunc::NEVER:
            return D3D_COMPARISON_(NEVER);
        case ComparisonFunc::LESS:
            return D3D_COMPARISON_(LESS);
        case ComparisonFunc::EQUAL:
            return D3D_COMPARISON_(EQUAL);
        case ComparisonFunc::LESS_EQUAL:
            return D3D_COMPARISON_(LESS_EQUAL);
        case ComparisonFunc::GREATER:
            return D3D_COMPARISON_(GREATER);
        case ComparisonFunc::GREATER_EQUAL:
            return D3D_COMPARISON_(GREATER_EQUAL);
        case ComparisonFunc::ALWAYS:
            return D3D_COMPARISON_(ALWAYS);
    }
    return D3D_COMPARISON_(NEVER);
}

static inline D3D_TEXTURE_ADDRESS_MODE Convert(AddressMode texture_address_mode) {
    switch (texture_address_mode) {
        case AddressMode::WRAP:
            return D3D_TEXTURE_ADDRESS_MODE_(WRAP);
        case AddressMode::CLAMP:
            return D3D_TEXTURE_ADDRESS_MODE_(CLAMP);
        case AddressMode::BORDER:
            return D3D_TEXTURE_ADDRESS_MODE_(BORDER);
        default:
            CRASH_NOW();
            return D3D_TEXTURE_ADDRESS_MODE_(WRAP);
    }
}

static inline D3D_FILTER Convert(FilterMode p_min_filter, FilterMode p_mag_filter) {
    // @TODO: refactor this
    if (p_min_filter == FilterMode::MIPMAP_LINEAR && p_mag_filter == FilterMode::MIPMAP_LINEAR) {
        return D3D_FILTER_(MIN_MAG_MIP_LINEAR);
    }
    if (p_min_filter == FilterMode::POINT && p_mag_filter == FilterMode::POINT) {
        return D3D_FILTER_(MIN_MAG_MIP_POINT);
    }
    if (p_min_filter == FilterMode::LINEAR && p_mag_filter == FilterMode::LINEAR) {
        return D3D_FILTER_(MIN_MAG_LINEAR_MIP_POINT);
    }

    CRASH_NOW_MSG(std::format("Unknown filter {} and {}", static_cast<int>(p_min_filter), static_cast<int>(p_mag_filter)));
    return D3D_FILTER_(MIN_MAG_MIP_POINT);
};

#if defined(INCLUDE_AS_D3D11)
static inline D3D11_USAGE Convert(BufferUsage p_usage) {
    switch (p_usage) {
        case BufferUsage::IMMUTABLE:
            return D3D11_USAGE_IMMUTABLE;
        case BufferUsage::DYNAMIC:
            return D3D11_USAGE_DYNAMIC;
        case BufferUsage::STAGING:
            return D3D11_USAGE_STAGING;
        case BufferUsage::DEFAULT:
            [[fallthrough]];
        default:
            return D3D11_USAGE_DEFAULT;
    }
}
#endif

#if defined(INCLUDE_AS_D3D12)
static inline D3D12_STATIC_BORDER_COLOR Convert(StaticBorderColor p_color) {
    switch (p_color) {
        case StaticBorderColor::TRANSPARENT_BLACK:
            return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        case StaticBorderColor::OPAQUE_BLACK:
            return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        case StaticBorderColor::OPAQUE_WHITE:
            return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        default:
            CRASH_NOW();
            return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    }
}
#endif

#if 0
static inline DEPTH_WRITE_MASK Convert(DepthWriteMask depth_write_mask) {
    switch (depth_write_mask) {
        case DepthWriteMask::ZERO:
            return D3D(DEPTH_WRITE_MASK_ZERO);
        case DepthWriteMask::ALL:
            return D3D(DEPTH_WRITE_MASK_ALL);
    }
    return D3D(DEPTH_WRITE_MASK_ZERO);
}
#endif

#undef D3D_
#undef D3D_INPUT_CLASSIFICATION_
#undef D3D_FILL_MODE_
#undef D3D_CULL_MODE_
#undef D3D_COMPARISON_FUNC_
#undef D3D_FILTER_
#undef D3D_TEXTURE_ADDRESS_MODE_

}  // namespace my::d3d
