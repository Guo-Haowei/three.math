#ifndef CBUFFER_INCLUDED
#define CBUFFER_INCLUDED
#include "shader_constants.h"

// constant buffer
#ifdef __cplusplus
#define CONSTANT_BUFFER(name, reg) \
    struct name : public ConstantBufferBase<name, reg>

template<typename T, int N>
struct ConstantBufferBase {
    ConstantBufferBase() {
        static_assert(sizeof(T) % 16 == 0);
    }
    constexpr int get_slot() { return N; }
};
#else
#define CONSTANT_BUFFER(name, reg) layout(std140, binding = reg) uniform name
#endif

// sampler
#ifdef __cplusplus
using sampler2D = uint64_t;
using sampler3D = uint64_t;
using samplerCube = uint64_t;
// struct samplerCube {
//     uint64_t data;
//     uint64_t padding;
// };
static_assert(MAX_CASCADE_COUNT == 4);
#endif

struct Light {
    vec3 color;
    int type;
    vec3 position;  // direction
    int cast_shadow;
    samplerCube shadow_map;
    float atten_constant;
    float atten_linear;

    vec2 padding;
    float atten_quadratic;
    float max_distance;  // max distance the light affects
    mat4 matrices[6];
};

CONSTANT_BUFFER(PerFrameConstantBuffer, 0) {
    mat4 c_view_matrix;
    mat4 c_projection_matrix;
    mat4 c_projection_view_matrix;

    Light c_lights[MAX_LIGHT_COUNT];

    // @TODO: move it to Light
    mat4 c_main_light_matrices[MAX_CASCADE_COUNT];
    vec4 c_cascade_plane_distances;

    int c_light_count;
    int c_enable_csm;
    int c_display_method;
    int _c_padding_0;

    int c_debug_voxel_id;
    int c_no_texture;
    int c_screen_width;
    int c_screen_height;

    vec3 c_camera_position;
    float c_voxel_size;

    vec3 c_world_center;
    float c_world_size_half;

    int c_ssao_kernel_size;
    float c_ssao_kernel_radius;
    int c_ssao_noise_size;
    float c_texel_size;

    int c_enable_ssao;
    int c_enable_fxaa;
    int c_enable_vxgi;
    int c_debug_csm;
};

CONSTANT_BUFFER(PerBatchConstantBuffer, 1) {
    mat4 c_projection_view_model_matrix;
    mat4 c_model_matrix;

    vec3 _c_ddddd_padding;
    int c_light_index;  // HACK: shouldn't be here
};

CONSTANT_BUFFER(MaterialConstantBuffer, 2) {
    vec4 c_albedo_color;
    float c_metallic;
    float c_roughness;
    float c_reflect_power;
    int c_has_albedo_map;

    vec2 _c_padding1;
    int c_has_pbr_map;
    int c_has_normal_map;

    sampler2D c_albedo_map;
    sampler2D c_normal_map;
    sampler2D c_pbr_map;
    sampler2D _c_dummy_padding;
};

CONSTANT_BUFFER(PerSceneConstantBuffer, 3) {
    vec4 c_ssao_kernels[MAX_SSAO_KERNEL_COUNT];

    sampler2D c_shadow_map;
    sampler2D c_skybox_map;
    sampler3D c_voxel_map;
    sampler3D c_voxel_normal_map;

    sampler2D c_gbuffer_albedo_map;
    sampler2D c_gbuffer_position_metallic_map;
    sampler2D c_gbuffer_normal_roughness_map;
    sampler2D c_gbuffer_depth_map;

    sampler2D c_ssao_map;
    sampler2D c_kernel_noise_map;
    sampler2D c_fxaa_image;
    sampler2D c_fxaa_input_image;
};

CONSTANT_BUFFER(BoneConstantBuffer, 4) {
    mat4 c_bones[MAX_BONE_COUNT];
};

// @TODO: make it more general, something like 2D draw
CONSTANT_BUFFER(DebugDrawConstantBuffer, 5) {
    vec2 c_debug_draw_pos;
    vec2 c_debug_draw_size;

    sampler2D c_debug_draw_map;
    int c_display_channel;
    int c_another_padding;
};

#endif