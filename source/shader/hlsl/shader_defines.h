#ifndef SHADER_DEFINES_INCLUDED
#define SHADER_DEFINES_INCLUDED

#ifndef MY_PI
#define MY_PI 3.141592653589793
#endif

#ifndef MY_TWO_PI
#define MY_TWO_PI 6.283185307179586
#endif

#define MAX_LIGHT_COUNT             16
#define MAX_BONE_COUNT              64
#define MAX_SSAO_KERNEL_COUNT       64
#define MAX_CASCADE_COUNT           4
#define MAX_LIGHT_CAST_SHADOW_COUNT 4
#define LIGHT_SHADOW_MIN_DISTANCE   0.1f

// light type
#define LIGHT_TYPE_OMNI  0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT  2
#define LIGHT_TYPE_AREA  3
#define LIGHT_TYPE_MAX   4

// display method
#define DISPLAY_CHANNEL_RGB 0
#define DISPLAY_CHANNEL_RRR 1
#define DISPLAY_CHANNEL_AAA 2

const float LUT_SIZE = 64.0;  // ltc_texture size
const float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
const float LUT_BIAS = 0.5 / LUT_SIZE;

#if defined(HLSL_LANG)
#define ivec2 int2
#define ivec3 int3
#define ivec4 int4
#define vec2  float2
#define vec3  float3
#define vec4  float4
#define mat3  float3x3
#define mat4  float4x4
#endif

#endif
