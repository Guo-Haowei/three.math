#pragma once
#include "render_graph.h"

#define SHADOW_PASS       "shadow_pass"
#define VOXELIZATION_PASS "voxelization_pass"
#define VXGI_DEBUG_PASS   "debug_vxgi_pass"
#define GBUFFER_PASS      "gbuffer_pass"
#define LIGHTING_PASS     "lighting_pass"
#define SSAO_PASS         "ssao_pass"
#define FXAA_PASS         "fxaa_pass"
#define FINAL_PASS        "final_pass"

#define SHADOW_PASS_OUTPUT           SHADOW_PASS "_output"
#define SSAO_PASS_OUTPUT             SSAO_PASS "_output"
#define FXAA_PASS_OUTPUT             FXAA_PASS "_output"
#define LIGHTING_PASS_OUTPUT         LIGHTING_PASS "_output"
#define GBUFFER_PASS_OUTPUT_POSITION "gbuffer_output_position"
#define GBUFFER_PASS_OUTPUT_NORMAL   "gbuffer_output_normal"
#define GBUFFER_PASS_OUTPUT_ALBEDO   "gbuffer_output_albedo"
#define GBUFFER_PASS_OUTPUT_DEPTH    "gbuffer_output_depth"
#define DEBUG_VXGI_OUTPUT_COLOR      "debug_vxgi_output_color"
#define DEBUG_VXGI_OUTPUT_DEPTH      "debug_vxgi_output_depth"

namespace my {

void shadow_pass_func();
void gbuffer_pass_func();
void voxelization_pass_func();
void debug_vxgi_pass_func();
void ssao_pass_func();
void lighting_pass_func();
void fxaa_pass_func();
void final_pass_func();

void create_render_graph_default(RenderGraph& graph);

}  // namespace my
