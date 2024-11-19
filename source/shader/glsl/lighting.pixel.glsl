/// File: lighting.pixel.glsl
#include "../cbuffer.hlsl.h"

#define ENABLE_VXGI 1

layout(location = 0) out vec3 out_color;
layout(location = 0) in vec2 pass_uv;

#include "lighting.glsl"

#if ENABLE_VXGI
#include "vxgi.glsl"
#endif

vec3 FresnelSchlickRoughness(float cosTheta, in vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness) - F0, vec3(0.0))) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    const vec2 texcoord = pass_uv;

    // @TODO: check if this is necessary
    float depth = texture(t_gbufferDepth, texcoord).r;

    if (depth > 0.9999) {
        discard;
    }
    gl_FragDepth = depth;

    vec3 N = texture(t_gbufferNormalMap, texcoord).rgb;
    // N = (2.0 * N) - vec3(1.0);

    const vec3 world_position = texture(t_gbufferPositionMap, texcoord).rgb;
    const vec3 emissive_roughness_metallic = texture(t_gbufferMaterialMap, texcoord).rgb;
    float emissive = emissive_roughness_metallic.r;
    float roughness = emissive_roughness_metallic.g;
    float metallic = emissive_roughness_metallic.b;

    vec3 base_color = texture(t_gbufferBaseColorMap, texcoord).rgb;
    if (c_noTexture != 0) {
        base_color = vec3(0.6);
    }

    if (emissive > 0.0) {
        out_color = emissive * base_color;
        return;
    }

    const vec3 V = normalize(c_cameraPosition - world_position);
    const float NdotV = clamp(dot(N, V), 0.0, 1.0);
    vec3 R = reflect(-V, N);

    vec3 Lo = vec3(0.0);
    vec3 F0 = mix(vec3(0.04), base_color, metallic);

    // ------------------- for area light
    // use roughness and sqrt(1-cos_theta) to sample M_texture
    vec2 uv = vec2(roughness, sqrt(1.0f - NdotV));
    uv = uv * LUT_SCALE + LUT_BIAS;

    // get 4 parameters for inverse_M
    vec4 t1 = texture(c_ltc1, uv);

    // Get 2 parameters for Fresnel calculation
    vec4 t2 = texture(c_ltc2, uv);

    mat3 Minv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(0, 1, 0),
        vec3(t1.z, 0, t1.w));
    // ------------------- for area light

    for (int light_idx = 0; light_idx < c_lightCount; ++light_idx) {
        Light light = c_lights[light_idx];
        int light_type = c_lights[light_idx].type;
        vec3 direct_lighting = vec3(0.0);
        float shadow = 0.0;
        const vec3 radiance = light.color;
        switch (light.type) {
            case LIGHT_TYPE_INFINITE: {
                vec3 L = light.position;
                float atten = 1.0;
                const vec3 H = normalize(V + L);
                direct_lighting = atten * lighting(N, L, V, radiance, F0, roughness, metallic, base_color);
                if (light.cast_shadow == 1) {
                    const float NdotL = max(dot(N, L), 0.0);
                    shadow = shadowTest(light, world_position, NdotL);
                    direct_lighting *= (1.0 - shadow);
                }
            } break;
            case LIGHT_TYPE_POINT: {
                vec3 delta = -world_position + light.position;
                float dist = length(delta);
                float atten = (light.atten_constant + light.atten_linear * dist +
                               light.atten_quadratic * (dist * dist));
                atten = 1.0 / atten;
                if (atten > 0.01) {
                    vec3 L = normalize(delta);
                    const vec3 H = normalize(V + L);
                    direct_lighting = atten * lighting(N, L, V, radiance, F0, roughness, metallic, base_color);
                    if (light.cast_shadow == 1) {
                        shadow = point_shadow_calculation(light, world_position, c_cameraPosition);
                    }
                }
            } break;
            case LIGHT_TYPE_AREA: {
                direct_lighting = radiance * area_light(Minv, N, V, world_position, t2, F0, base_color, light.points);
            } break;
            default:
                break;
        }
        Lo += (1.0 - shadow) * direct_lighting;
    }

    // ambient

    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    vec3 irradiance = texture(c_diffuseIrradianceMap, N).rgb;
    vec3 diffuse = irradiance * base_color.rgb;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(c_prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(c_brdfMap, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    const float ao = 1.0;
    vec3 ambient = (kD * diffuse + specular) * ao;

#if ENABLE_VXGI
    if (c_enableVxgi == 1) {
        const vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        const vec3 kS = F;
        const vec3 kD = (1.0 - kS) * (1.0 - metallic);

        // indirect diffuse
        vec3 diffuse = base_color.rgb * cone_diffuse(world_position, N);

        // specular cone
        vec3 coneDirection = reflect(-V, N);
        vec3 specular = vec3(0);
        specular = metallic * cone_specular(world_position, coneDirection, roughness);
        Lo += (kD * diffuse + specular) * ao;
    }
#endif

    vec3 color = Lo + ambient;
    out_color = color;
}
