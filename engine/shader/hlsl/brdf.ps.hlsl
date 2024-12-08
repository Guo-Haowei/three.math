/// File: brdf.ps.hlsl
#include "hlsl/input_output.hlsl"
#include "pbr.hlsl.h"
#include "shader_defines.hlsl.h"

float2 main(vsoutput_uv input) : SV_TARGET {
    Vector2f integratedBRDF = IntegrateBRDF(input.uv.x, input.uv.y);
    return integratedBRDF;
}
