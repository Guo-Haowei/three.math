#if defined(__cplusplus)
#ifndef SHADER_TEXTURE
#define SHADER_TEXTURE(...)
#endif
#elif defined(HLSL_LANG)
#define SHADER_TEXTURE(TYPE, NAME, SLOT) TYPE NAME : register(t##SLOT);
#else
#define SHADER_TEXTURE(TYPE, NAME, SLOT) uniform TYPE NAME;
#define Texture2D                        sampler2D
#define TextureCube                      samplerCube
#endif

//@TODO: refactor this
SHADER_TEXTURE(Texture2D, u_base_color_map, 0)
SHADER_TEXTURE(Texture2D, u_normal_map, 1)
SHADER_TEXTURE(Texture2D, u_material_map, 2)

SHADER_TEXTURE(Texture2D, t_gbufferDepth, 8)

SHADER_TEXTURE(Texture2D, u_selection_highlight, 9)
SHADER_TEXTURE(Texture2D, u_gbuffer_base_color_map, 10)
SHADER_TEXTURE(Texture2D, u_gbuffer_position_map, 11)
SHADER_TEXTURE(Texture2D, u_gbuffer_normal_map, 12)
SHADER_TEXTURE(Texture2D, u_gbuffer_material_map, 13)
SHADER_TEXTURE(Texture2D, g_texture_lighting, 14)
SHADER_TEXTURE(Texture2D, g_bloom_input_image, 15)

// [SCRUM-34] @TODO: shadow atlas?
SHADER_TEXTURE(TextureCube, t_point_shadow_0, 21)
SHADER_TEXTURE(TextureCube, t_point_shadow_1, 22)
SHADER_TEXTURE(TextureCube, t_point_shadow_2, 23)
SHADER_TEXTURE(TextureCube, t_point_shadow_3, 24)
SHADER_TEXTURE(Texture2D, t_shadow_map, 25)
