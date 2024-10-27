#include "cbuffer.h"
#include "hlsl/input_output.hlsl"

vsoutput_position main(vsinput_mesh input) {
#ifdef HAS_ANIMATION
    float4x4 boneTransform = u_bones[input.boneIndex.x] * input.boneWeight.x;
    boneTransform += u_bones[input.boneIndex.y] * input.boneWeight.y;
    boneTransform += u_bones[input.boneIndex.z] * input.boneWeight.z;
    boneTransform += u_bones[input.boneIndex.w] * input.boneWeight.w;
    float4x4 world_matrix = mul(u_world_matrix, boneTransform);
#else
    float4x4 world_matrix = u_world_matrix;
#endif

    float4 position = mul(world_matrix, float4(input.position, 1.0));
    vsoutput_position output;
    output.world_position = position.xyz;
    output.position = mul(g_point_light_matrix, position);
    return output;
}
