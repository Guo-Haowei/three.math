#include "opengl_pipeline_state_manager.h"

#include "core/framework/asset_manager.h"

namespace my {

namespace fs = std::filesystem;

static auto process_shader(const fs::path &p_path, int p_depth) -> std::expected<std::string, std::string> {
    constexpr int max_depth = 100;
    if (p_depth >= max_depth) {
        return std::unexpected("circular includes!");
    }

    auto source_binary = AssetManager::singleton().load_file_sync(p_path.string());
    if (source_binary->buffer.empty()) {
        return std::unexpected(std::format("failed to read file '{}'", p_path.string()));
    }

    std::string source(source_binary->buffer.begin(), source_binary->buffer.end());

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

            fs::path new_path = p_path;
            new_path.remove_filename();
            new_path = new_path / file_to_include;

            auto res = process_shader(new_path, p_depth + 1);
            if (!res) {
                return res.error();
            }

            result.append(*res);
        } else {
            result.append(line);
        }

        result.push_back('\n');
    }

    return result;
}

static GLuint create_shader(std::string_view p_file, GLenum p_type, const std::vector<ShaderMacro> &p_defines) {
    std::string file{ p_file };
    file.append(".glsl");
    fs::path path = fs::path{ ROOT_FOLDER } / "source" / "shader" / "glsl" / file;
    auto res = process_shader(path, 0);
    if (!res) {
        LOG_ERROR("Failed to create shader '{}', reason: {}", p_file, res.error());
        return 0;
    }

    // @TODO: fix this
    std::string fullsource =
        "#version 460 core\n"
        "#extension GL_NV_gpu_shader5 : require\n"
        "#extension GL_NV_shader_atomic_float : enable\n"
        "#extension GL_NV_shader_atomic_fp16_vector : enable\n"
        "#extension GL_ARB_bindless_texture : require\n"
        "";

    for (const auto &define : p_defines) {
        fullsource.append(std::format("#define {} {}\n", define.name, define.value));
    }

    fullsource.append(*res);
    const char *sources[] = { fullsource.c_str() };

    GLuint shader = glCreateShader(p_type);
    glShaderSource(shader, 1, sources, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE, length = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        std::vector<char> buffer(length + 1);
        glGetShaderInfoLog(shader, length, nullptr, buffer.data());
        LOG_FATAL("[glsl] failed to compile shader '{}'\ndetails:\n{}", p_file, buffer.data());
    }

    if (status == GL_FALSE) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

std::shared_ptr<PipelineState> OpenGLPipelineStateManager::create(const PipelineCreateInfo &p_info) {
    GLuint programID = glCreateProgram();
    std::vector<GLuint> shaders;
    auto create_shader_helper = [&](std::string_view path, GLenum type) {
        if (!path.empty()) {
            GLuint shader = create_shader(path, type, p_info.defines);
            glAttachShader(programID, shader);
            shaders.push_back(shader);
        }
    };

    ON_SCOPE_EXIT([&]() {
        for (GLuint id : shaders) {
            glDeleteShader(id);
        }
    });

    if (!p_info.cs.empty()) {
        DEV_ASSERT(p_info.vs.empty());
        DEV_ASSERT(p_info.ps.empty());
        DEV_ASSERT(p_info.gs.empty());
        create_shader_helper(p_info.cs, GL_COMPUTE_SHADER);
    } else if (!p_info.vs.empty()) {
        DEV_ASSERT(p_info.cs.empty());
        create_shader_helper(p_info.vs, GL_VERTEX_SHADER);
        create_shader_helper(p_info.ps, GL_FRAGMENT_SHADER);
        create_shader_helper(p_info.gs, GL_GEOMETRY_SHADER);
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

    auto program = std::make_shared<OpenGLPipelineState>();
    program->program_id = programID;
    return program;
}

}  // namespace my
