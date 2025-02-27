/// File: bloom_upsample.cs.hlsl
#include "cbuffer.hlsl.h"
#include "sampler.hlsl.h"
#include "shader_resource_defines.hlsl.h"
#include "unordered_access_defines.hlsl.h"

[numthreads(16, 16, 1)] void main(uint3 dispatch_thread_id : SV_DISPATCHTHREADID) {
    const uint2 output_coord = dispatch_thread_id.xy;

    uint width, height;
    RW_TEXTURE_2D(BloomOutputImage).GetDimensions(width, height);
    float2 output_image_size = float2(width, height);

    float2 uv = float2(output_coord.x / output_image_size.x,
                       output_coord.y / output_image_size.y);

    uint input_width, input_height;
    TEXTURE_2D(BloomInputTexture).GetDimensions(input_width, input_height);
    float x = 1.0f / input_width;
    float y = 1.0f / input_height;
    uv.x += 0.5f * x;
    uv.y += 0.5f * y;

    float filterRadius = 0.005;
    x = filterRadius;
    y = filterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x - x, uv.y + y), 0).rgb;
    float3 b = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x, uv.y + y), 0).rgb;
    float3 c = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x + x, uv.y + y), 0).rgb;

    float3 d = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x - x, uv.y), 0).rgb;
    float3 e = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x, uv.y), 0).rgb;
    float3 f = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x + x, uv.y), 0).rgb;

    float3 g = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x - x, uv.y - y), 0).rgb;
    float3 h = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x, uv.y - y), 0).rgb;
    float3 i = TEXTURE_2D(BloomInputTexture).SampleLevel(s_linearClampSampler, float2(uv.x + x, uv.y - y), 0).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    float3 upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i);
    upsample *= 1.0 / 16.0;

    float3 final_color = RW_TEXTURE_2D(BloomOutputImage).Load(output_coord).rgb;
    final_color = lerp(final_color, upsample, 0.6);
    RW_TEXTURE_2D(BloomOutputImage)
    [output_coord] = float4(final_color, 1.0);
}
