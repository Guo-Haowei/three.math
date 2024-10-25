#include "texture_binding.h"

RWTexture2D<float3> g_output_image : register(u3);

// @TODO: fix this
SamplerState u_sampler : register(s1);

[numthreads(16, 16, 1)] void main(uint3 dispatch_thread_id
                                  : SV_DISPATCHTHREADID) {
    const uint2 output_coord = dispatch_thread_id.xy;

    uint width, height;
    g_output_image.GetDimensions(width, height);
    float2 output_image_size = float2(width, height);

    float2 uv = float2(output_coord.x / output_image_size.x,
                       output_coord.y / output_image_size.y);

    uint input_width, input_height;
    g_bloom_input_image.GetDimensions(input_width, input_height);
    float x = 1.0f / input_width;
    float y = 1.0f / input_height;
    uv.x += 0.5f * x;
    uv.y += 0.5f * y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x - 2 * x, uv.y + 2 * y), 0).rgb;
    float3 b = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x, uv.y + 2 * y), 0).rgb;
    float3 c = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x + 2 * x, uv.y + 2 * y), 0).rgb;

    float3 d = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x - 2 * x, uv.y), 0).rgb;
    float3 e = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x, uv.y), 0).rgb;
    float3 f = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x + 2 * x, uv.y), 0).rgb;

    float3 g = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x - 2 * x, uv.y - 2 * y), 0).rgb;
    float3 h = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x, uv.y - 2 * y), 0).rgb;
    float3 i = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x + 2 * x, uv.y - 2 * y), 0).rgb;

    float3 j = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x - x, uv.y + y), 0).rgb;
    float3 k = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x + x, uv.y + y), 0).rgb;
    float3 l = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x - x, uv.y - y), 0).rgb;
    float3 m = g_bloom_input_image.SampleLevel(u_sampler, float2(uv.x + x, uv.y - y), 0).rgb;

    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    float3 final_color = e * 0.125;
    final_color += (a + c + g + i) * 0.03125;
    final_color += (b + d + f + h) * 0.0625;
    final_color += (j + k + l + m) * 0.125;

    final_color = max(final_color, 0.000f);
    g_output_image[output_coord] = final_color;
}