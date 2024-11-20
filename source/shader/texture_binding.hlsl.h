/// File: texture_binding.hlsl.h
#if defined(__cplusplus)
#ifndef SHADER_TEXTURE
#define SHADER_TEXTURE(...)
#endif
#elif defined(HLSL_LANG)
#define SHADER_TEXTURE(TYPE, NAME, SLOT, BINDING) TYPE NAME : register(t##SLOT);
#else
#define SHADER_TEXTURE(TYPE, NAME, SLOT, BINDING) uniform TYPE NAME;
#define Texture2D                                 sampler2D
#define TextureCube                               samplerCube
#define TextureCubeArray                          samplerCubeArray
#endif

#if defined(HLSL_LANG_D3D12)
Texture2D t_texture2Ds[MAX_TEXTURE_2D_COUNT] : register(t0, space0);
TextureCubeArray t_textureCubeArrays[MAX_TEXTURE_CUBE_ARRAY_COUNT] : register(t0, space1);
#else
// dynamic srvs
SHADER_TEXTURE(Texture2D, t_baseColorMap, 0, RESOURCE_NONE)
SHADER_TEXTURE(Texture2D, t_normalMap, 1, RESOURCE_NONE)
SHADER_TEXTURE(Texture2D, t_materialMap, 2, RESOURCE_NONE)
SHADER_TEXTURE(Texture2D, t_bloomInputImage, 3, RESOURCE_NONE)

// static srvs
SHADER_TEXTURE(Texture2D, t_textureHighlightSelect, 8, RESOURCE_HIGHLIGHT_SELECT)
SHADER_TEXTURE(Texture2D, t_gbufferDepth, 9, RESOURCE_GBUFFER_DEPTH)
SHADER_TEXTURE(Texture2D, t_gbufferBaseColorMap, 10, RESOURCE_GBUFFER_BASE_COLOR)
SHADER_TEXTURE(Texture2D, t_gbufferPositionMap, 11, RESOURCE_GBUFFER_POSITION)
SHADER_TEXTURE(Texture2D, t_gbufferNormalMap, 12, RESOURCE_GBUFFER_NORMAL)
SHADER_TEXTURE(Texture2D, t_gbufferMaterialMap, 13, RESOURCE_GBUFFER_MATERIAL)
SHADER_TEXTURE(Texture2D, t_textureLighting, 14, RESOURCE_LIGHTING)

SHADER_TEXTURE(TextureCubeArray, t_pointShadowArray, 21, RESOURCE_NONE)
// @TODO: make it an array as well
SHADER_TEXTURE(Texture2D, t_shadowMap, 25, RESOURCE_NONE)
#endif

// @TODO: refactor
#if defined(HLSL_LANG_D3D12)
#define TEXTURE_2D(NAME)         (t_texture2Ds[c_##NAME##Index])
#define TEXTURE_CUBE_ARRAY(NAME) (t_textureCubeArrays[c_##NAME##Index])
#elif defined(HLSL_LANG_D3D11)
#define TEXTURE_2D(NAME)         (t_##NAME)
#define TEXTURE_CUBE_ARRAY(NAME) (t_##NAME)
#endif
