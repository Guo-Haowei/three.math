/// File: skybox.pixel.glsl
#include "../cbuffer.hlsl.h"

in vec3 pass_position;
out vec4 out_color;

void main() {
    vec3 color = texture(c_envMap, pass_position).rgb;
    // vec3 color = texture(c_diffuseIrradianceMap, pass_position).rgb;
    // vec3 color = textureLod(c_prefilteredMap, pass_position, 1.0).rgb;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    out_color = vec4(color, 1.0);
}