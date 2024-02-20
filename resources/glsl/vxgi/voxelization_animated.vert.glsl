#include "cbuffer.glsl.h"
#include "vsinput.glsl.h"

out vec3 pass_positions;
out vec3 pass_normals;
out vec2 pass_uvs;

void main() {
    mat4 bone_matrix = c_bones[in_bone_id.x] * in_bone_weight.x;
    bone_matrix += c_bones[in_bone_id.y] * in_bone_weight.y;
    bone_matrix += c_bones[in_bone_id.z] * in_bone_weight.z;
    bone_matrix += c_bones[in_bone_id.w] * in_bone_weight.w;

    // assume no transformation
    vec4 world_position = c_model_matrix * bone_matrix * vec4(in_position, 1.0);
    pass_positions = world_position.xyz;
    pass_normals = normalize(in_normal);
    pass_uvs = in_uv;
    gl_Position = world_position;
}
