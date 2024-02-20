#include "shader_program_manager.h"

#include <sstream>
#include <vector>

#include "core/framework/asset_manager.h"
#include "rendering/GLPrerequisites.h"

namespace my {

// @TODO: fix
static ShaderProgramManager *g_shader_program_manager = new ShaderProgramManager();
static std::vector<ShaderProgram> s_shader_cache;

static std::string process_shader(const std::string &source, int depth) {
    constexpr int max_depth = 100;
    if (depth >= max_depth) {
        LOG_FATAL("maximum include depth {} exceeded", max_depth);
    }

    std::string result;
    std::stringstream ss(source);
    for (std::string line; std::getline(ss, line);) {
        constexpr const char pattern[] = "#include";
        if (line.find(pattern) == 0) {
            const char *lineStr = line.c_str();
            const char *quote1 = strchr(lineStr, '"');
            const char *quote2 = strrchr(lineStr, '"');
            DEV_ASSERT(quote1 && quote2 && (quote1 != quote2));
            std::string file_to_include(quote1 + 1, quote2);

            file_to_include = "@res://glsl/" + file_to_include;
            // @TODO: nested include
            auto buffer = AssetManager::singleton().load_file_sync(file_to_include);
            DEV_ASSERT(buffer);
            std::string extra(buffer->buffer.begin(), buffer->buffer.end());
            if (extra.empty()) {
                LOG_ERROR("[filesystem] failed to read shader '{}'", file_to_include);
            }
            result.append(process_shader(extra, depth + 1));
        } else {
            result.append(line);
        }

        result.push_back('\n');
    }

    return result;
}

static GLuint create_shader(std::string_view file, GLenum type) {
    auto source_binary = AssetManager::singleton().load_file_sync(std::string(file));
    DEV_ASSERT(source_binary);

    std::string source(source_binary->buffer.begin(), source_binary->buffer.end());
    if (source.empty()) {
        LOG_ERROR("[filesystem] failed to read shader '{}'", file);
        return 0;
    }

    std::string fullsource =
        "#version 460 core\n"
        "#extension GL_NV_gpu_shader5 : require\n"
        "#extension GL_NV_shader_atomic_float : enable\n"
        "#extension GL_NV_shader_atomic_fp16_vector : enable\n"
        "#extension GL_ARB_bindless_texture : require\n"
        "";

    fullsource.append(process_shader(source, 0));
    const char *sources[] = { fullsource.c_str() };

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, sources, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE, length = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        std::vector<char> buffer(length + 1);
        glGetShaderInfoLog(shader, length, nullptr, buffer.data());
        LOG_FATAL("[glsl] failed to compile shader '{}'\ndetails:\n{}", file, buffer.data());
    }

    if (status == GL_FALSE) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

ShaderProgram ShaderProgramManager::create(const ProgramCreateInfo &info) {
    GLuint programID = glCreateProgram();
    std::vector<GLuint> shaders;
    auto create_shader_helper = [&](std::string_view path, GLenum type) {
        if (!path.empty()) {
            GLuint shader = create_shader(path, type);
            glAttachShader(programID, shader);
            shaders.push_back(shader);
        }
    };

    ON_SCOPE_EXIT([&]() {
        for (GLuint id : shaders) {
            glDeleteShader(id);
        }
    });

    if (!info.cs.empty()) {
        DEV_ASSERT(info.vs.empty());
        DEV_ASSERT(info.ps.empty());
        DEV_ASSERT(info.gs.empty());
        create_shader_helper(info.cs, GL_COMPUTE_SHADER);
    } else if (!info.vs.empty()) {
        DEV_ASSERT(info.cs.empty());
        create_shader_helper(info.vs, GL_VERTEX_SHADER);
        create_shader_helper(info.ps, GL_FRAGMENT_SHADER);
        create_shader_helper(info.gs, GL_GEOMETRY_SHADER);
    }

    DEV_ASSERT(!shaders.empty());

    glLinkProgram(programID);
    GLint status = GL_FALSE, length = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &status);
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        std::vector<char> buffer(length + 1);
        glGetProgramInfoLog(programID, length, nullptr, buffer.data());
        LOG_FATAL("[glsl] failed to link program\ndetails:\n{}", buffer.data());
    }

    if (status == GL_FALSE) {
        glDeleteProgram(programID);
        programID = 0;
    }

    ShaderProgram program;
    program.m_handle = programID;
    return program;
}

bool ShaderProgramManager::initialize() {
    s_shader_cache.resize(static_cast<int>(ProgramType::PROGRAM_MAX));
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/mesh_static.vert";
        info.ps = "@res://glsl/gbuffer.frag";
        s_shader_cache[PROGRAM_GBUFFER_STATIC] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/mesh_animated.vert";
        info.ps = "@res://glsl/gbuffer.frag";
        s_shader_cache[PROGRAM_GBUFFER_ANIMATED] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/shadow_static.vert.glsl";
        info.ps = "@res://glsl/depth.frag";
        s_shader_cache[PROGRAM_DPETH_STATIC] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/shadow_animated.vert.glsl";
        info.ps = "@res://glsl/depth.frag";
        s_shader_cache[PROGRAM_DPETH_ANIMATED] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/point_shadow_static.vert.glsl";
        info.gs = "@res://glsl/point_shadow.geom.glsl";
        info.ps = "@res://glsl/point_shadow.frag.glsl";
        s_shader_cache[PROGRAM_POINT_SHADOW_STATIC] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/point_shadow_animated.vert.glsl";
        info.gs = "@res://glsl/point_shadow.geom.glsl";
        info.ps = "@res://glsl/point_shadow.frag.glsl";
        s_shader_cache[PROGRAM_POINT_SHADOW_ANIMATED] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/fullscreen.vert";
        info.ps = "@res://glsl/ssao.frag";
        s_shader_cache[PROGRAM_SSAO] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/fullscreen.vert";
        info.ps = "@res://glsl/lighting.frag.glsl";
        s_shader_cache[PROGRAM_LIGHTING_VXGI] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/fullscreen.vert";
        info.ps = "@res://glsl/fxaa.frag";
        s_shader_cache[PROGRAM_FXAA] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/vxgi/voxelization_static.vert.glsl";
        info.gs = "@res://glsl/vxgi/voxelization.geom.glsl";
        info.ps = "@res://glsl/vxgi/voxelization.frag.glsl";
        s_shader_cache[PROGRAM_VOXELIZATION_STATIC] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/vxgi/voxelization_animated.vert.glsl";
        info.gs = "@res://glsl/vxgi/voxelization.geom.glsl";
        info.ps = "@res://glsl/vxgi/voxelization.frag.glsl";
        s_shader_cache[PROGRAM_VOXELIZATION_ANIMATED] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.cs = "@res://glsl/vxgi/post.comp.glsl";
        s_shader_cache[PROGRAM_VOXELIZATION_POST] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/vxgi/visualization.vert.glsl";
        info.ps = "@res://glsl/vxgi/visualization.frag.glsl";
        s_shader_cache[PROGRAM_DEBUG_VOXEL] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/cube_map.vert.glsl";
        info.ps = "@res://glsl/cube_map.frag.glsl";
        s_shader_cache[PROGRAM_SKY_BOX] = create(info);
    }
    {
        ProgramCreateInfo info;
        info.vs = "@res://glsl/billboard.vert.glsl";
        info.ps = "@res://glsl/texture.frag.glsl";
        s_shader_cache[PROGRAM_BILLBOARD] = create(info);
    }

    return true;
}

void ShaderProgramManager::finalize() {
    for (auto it : s_shader_cache) {
        glDeleteProgram(it.m_handle);
    }
}

const ShaderProgram &ShaderProgramManager::get(ProgramType type) {
    DEV_ASSERT_INDEX(type, s_shader_cache.size());
    return s_shader_cache[std::to_underlying(type)];
}

}  // namespace my
