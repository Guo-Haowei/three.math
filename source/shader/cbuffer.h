#ifndef CBUFFER_INCLUDED
#define CBUFFER_INCLUDED
#include "shader_defines.h"

// constant buffer
#if defined(__cplusplus)

template<typename T, int N>
struct ConstantBufferBase {
    ConstantBufferBase() {
        static_assert(sizeof(T) % 16 == 0);
    }
    constexpr int GetSlot() { return N; }
    static constexpr int GetUniformBufferSlot() { return N; }
};

#define CBUFFER(NAME, REG) \
    struct NAME : public ConstantBufferBase<NAME, REG>

using sampler2D = uint64_t;
using sampler3D = uint64_t;
using samplerCube = uint64_t;

using TextureHandle = uint64_t;

// @TODO: remove this constraint
#elif defined(HLSL_LANG)
#define CBUFFER(NAME, REG) cbuffer NAME : register(b##REG)

#define TextureHandle float2
#define sampler2D     float2
#define samplerCube   float2

#elif defined(GLSL_LANG)
#define CBUFFER(NAME, REG) layout(std140, binding = REG) uniform NAME

#define TextureHandle vec2

#endif

struct Light {
    mat4 projection_matrix;  // 64
    mat4 view_matrix;        // 64
    vec4 points[4];          // 64

    vec3 color;
    int type;

    vec3 position;  // direction
    int cast_shadow;

    float atten_constant;
    float atten_linear;

    float atten_quadratic;
    float max_distance;  // max distance the light affects

    vec3 padding;
    int shadow_map_index;
};

CBUFFER(PerBatchConstantBuffer, 0) {
    mat4 c_worldMatrix;
    mat4 _per_batch_padding_0;
    mat4 _per_batch_padding_1;
    mat4 _per_batch_padding_2;
};

CBUFFER(PerPassConstantBuffer, 1) {
    mat4 c_viewMatrix;
    mat4 c_projectionMatrix;

    mat4 _per_pass_padding_0;
    mat4 _per_pass_padding_1;
};

CBUFFER(PerFrameConstantBuffer, 2) {
    Light c_lights[MAX_LIGHT_COUNT];

    int c_lightCount;
    int c_enableBloom;
    int c_debugCsm;
    float c_bloomThreshold;

    int c_debugVoxelId;
    int c_noTexture;
    int c_enableVxgi;
    float c_texelSize;

    vec3 c_cameraPosition;
    float c_voxelSize;

    vec3 c_worldCenter;
    float c_worldSizeHalf;

    vec4 _per_frame_padding_0;
};

CBUFFER(MaterialConstantBuffer, 3) {
    vec4 c_baseColor;

    float c_metallic;
    float c_roughness;
    float c_reflectPower;
    float c_emissivePower;

    int c_hasBaseColorMap;
    int c_hasPbrMap;
    int c_hasNormalMap;
    int c_hasHeightMap;

    TextureHandle u_base_color_map_handle;
    TextureHandle u_normal_map_handle;

    TextureHandle u_material_map_handle;
    TextureHandle u_height_map_handle;

    vec4 _material_padding_0;
    vec4 _material_padding_1;
    vec4 _material_padding_2;
    mat4 _material_padding_3;
    mat4 _material_padding_4;
};

// @TODO: change to unordered access buffer
CBUFFER(BoneConstantBuffer, 4) {
    mat4 c_bones[MAX_BONE_COUNT];
};

// @TODO: refactor name
CBUFFER(EmitterConstantBuffer, 5) {
    int c_preSimIdx;
    int c_postSimIdx;
    float c_elapsedTime;
    float c_lifeSpan;

    vec3 c_seeds;
    float c_emitterScale;
    vec3 c_emitterPosition;
    int c_particlesPerFrame;
    vec3 c_emitterStartingVelocity;
    int c_emitterMaxParticleCount;

    mat4 _emitter_padding_0;
    mat4 _emitter_padding_1;
    mat4 _emitter_padding_2;
};

#ifndef HLSL_LANG

CBUFFER(PerSceneConstantBuffer, 6) {
    // @TODO: remove the following
    sampler2D _per_scene_padding_0;
    sampler2D c_finalBloom;

    sampler2D c_grassBaseColor;
    sampler2D c_hdrEnvMap;
    sampler3D c_voxelMap;
    sampler3D c_voxelNormalMap;

    sampler2D c_kernelNoiseMap;
    sampler2D c_toneImage;
    // @TODO: unordered access
    sampler2D c_ltc1;
    sampler2D c_ltc2;

    sampler2D c_brdfMap;
    samplerCube c_envMap;
    samplerCube c_diffuseIrradianceMap;
    samplerCube c_prefilteredMap;
};

// @TODO: make it more general, something like 2D draw
CBUFFER(DebugDrawConstantBuffer, 7) {
    vec2 c_debugDrawPos;
    vec2 c_debugDrawSize;

    sampler2D c_debugDrawMap;
    int c_displayChannel;
    int _debug_draw_padding_0;
};
#endif

CBUFFER(PointShadowConstantBuffer, 8) {
    mat4 c_pointLightMatrix;    // 64
    vec3 c_pointLightPosition;  // 12
    float c_pointLightFar;      // 4

    vec4 _point_shadow_padding_0;  // 16
    vec4 _point_shadow_padding_1;  // 16
    vec4 _point_shadow_padding_2;  // 16

    mat4 _point_shadow_padding_3;  // 64
    mat4 _point_shadow_padding_4;  // 64
};

// @TODO: refactor this
CBUFFER(EnvConstantBuffer, 9) {
    mat4 c_cubeProjectionViewMatrix;

    float c_envPassRoughness;  // for environment map
    float _env_padding_0;
    float _env_padding_1;
    float _env_padding_2;

    vec4 _env_padding_3;
    vec4 _env_padding_4;
    vec4 _env_padding_5;

    mat4 _env_padding_6;
    mat4 _env_padding_7;
};

#endif