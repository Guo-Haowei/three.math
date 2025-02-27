/// File: input_output.hlsl
// Input
struct vsinput_position {
    float3 position : POSITION;
};

struct vsinput_mesh {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangent : TANGENT;
    int4 boneIndex : BONEINDEX;
    float4 boneWeight : BONEWEIGHT;
    float4 color : COLOR;
};

// Output
struct vsoutput_uv {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct vsoutput_position {
    float4 position : SV_POSITION;
    float3 world_position : POSITION;
};

struct vsinput_color {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct vsoutput_color {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

struct vsoutput_mesh {
    float4 position : SV_POSITION;
    float3 world_position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 T : TANGENT;
    float3 B : BITANGENT;
};
