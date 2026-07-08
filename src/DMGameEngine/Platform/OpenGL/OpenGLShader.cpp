/*
 * DMGameEngine - OpenGL Shader Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLShader.h"

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <array>

namespace DMGameEngine {

// ── Helper: shader type name ──────────────────────────────────────

namespace {

const char* ShaderTypeName(GLenum type)
{
    switch (type)
    {
        case GL_VERTEX_SHADER:          return "vertex";
        case GL_FRAGMENT_SHADER:        return "fragment";
        case GL_GEOMETRY_SHADER:        return "geometry";
        case GL_TESS_CONTROL_SHADER:    return "tessellation control";
        case GL_TESS_EVALUATION_SHADER: return "tessellation evaluation";
        case GL_COMPUTE_SHADER:         return "compute";
        default:                        return "unknown";
    }
}

GLenum ShaderTypeFromToken(std::string_view token)
{
    if (token == "vertex")                       return GL_VERTEX_SHADER;
    if (token == "fragment" || token == "pixel") return GL_FRAGMENT_SHADER;
    if (token == "geometry")                     return GL_GEOMETRY_SHADER;
    if (token == "tess_ctrl")                    return GL_TESS_CONTROL_SHADER;
    if (token == "tess_eval")                    return GL_TESS_EVALUATION_SHADER;
    if (token == "compute")                      return GL_COMPUTE_SHADER;

    DMGE_CORE_ASSERT(false, "Unknown shader type token: {0}", token);
    return 0;
}

} // anonymous namespace

// ── Constructors / Destructor ────────────────────────────────────

OpenGLShader::OpenGLShader(std::string_view name,
                           std::string_view vertexSrc,
                           std::string_view fragmentSrc)
    : m_Name(name)
{
    std::unordered_map<GLenum, std::string> sources;
    sources[GL_VERTEX_SHADER]   = std::string(vertexSrc);
    sources[GL_FRAGMENT_SHADER] = std::string(fragmentSrc);
    Compile(sources);
}

OpenGLShader::OpenGLShader(std::string_view filepath)
    : m_FilePath(filepath)
{
    std::string source = ReadFile(filepath);
    auto shaderSources = PreProcess(source);
    Compile(shaderSources);

    // Extract name from filepath (e.g. "assets/shaders/FlatColor.glsl" → "FlatColor")
    auto lastSlash = m_FilePath.find_last_of("/\\");
    auto lastDot   = m_FilePath.rfind('.');
    auto start     = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    auto count     = (lastDot == std::string::npos || lastDot < start)
                         ? std::string::npos
                         : lastDot - start;
    m_Name = m_FilePath.substr(start, count);
}

OpenGLShader::~OpenGLShader()
{
    DMGE_GL_CALL(glDeleteProgram(m_RendererID));
}

// ── Bind / Unbind ────────────────────────────────────────────────

void OpenGLShader::Bind() const
{
    DMGE_GL_CALL(glUseProgram(m_RendererID));
}

void OpenGLShader::Unbind() const
{
    DMGE_GL_CALL(glUseProgram(0));
}

// ── Uniform Setters ──────────────────────────────────────────────

void OpenGLShader::SetInt(std::string_view name, int value)
{
    DMGE_GL_CALL(glUniform1i(GetUniformLocation(name), value));
}

void OpenGLShader::SetIntArray(std::string_view name, const int* values, uint32_t count)
{
    DMGE_GL_CALL(glUniform1iv(GetUniformLocation(name), static_cast<GLsizei>(count), values));
}

void OpenGLShader::SetFloat(std::string_view name, float value)
{
    DMGE_GL_CALL(glUniform1f(GetUniformLocation(name), value));
}

void OpenGLShader::SetFloat2(std::string_view name, const glm::vec2& value)
{
    DMGE_GL_CALL(glUniform2f(GetUniformLocation(name), value.x, value.y));
}

void OpenGLShader::SetFloat3(std::string_view name, const glm::vec3& value)
{
    DMGE_GL_CALL(glUniform3f(GetUniformLocation(name), value.x, value.y, value.z));
}

void OpenGLShader::SetFloat4(std::string_view name, const glm::vec4& value)
{
    DMGE_GL_CALL(glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w));
}

void OpenGLShader::SetMat4(std::string_view name, const glm::mat4& value)
{
    DMGE_GL_CALL(glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value)));
}

// ── File I/O ─────────────────────────────────────────────────────

std::string OpenGLShader::ReadFile(std::string_view filepath) const
{
    std::ifstream in(std::string(filepath), std::ios::in | std::ios::binary);
    DMGE_CORE_ASSERT(in, "Could not open shader file: {0}", filepath);

    std::ostringstream content;
    content << in.rdbuf();
    in.close();
    return content.str();
}

// ── Shader Pre-Processing ────────────────────────────────────────

std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(std::string_view source) const
{
    std::unordered_map<GLenum, std::string> shaderSources;

    constexpr std::string_view typeToken = "#type";
    size_t pos = source.find(typeToken, 0);

    while (pos != std::string::npos)
    {
        size_t eol = source.find_first_of("\r\n", pos);
        DMGE_CORE_ASSERT(eol != std::string::npos, "Syntax error in shader source");

        size_t begin = pos + typeToken.size() + 1; // skip "#type "
        std::string_view type = source.substr(begin, eol - begin);

        size_t nextLinePos = source.find_first_not_of("\r\n", eol);
        DMGE_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error in shader source");

        pos = source.find(typeToken, nextLinePos);

        shaderSources[ShaderTypeFromToken(type)] =
            (pos == std::string::npos)
                ? std::string(source.substr(nextLinePos))
                : std::string(source.substr(nextLinePos, pos - nextLinePos));
    }

    return shaderSources;
}

// ── Compilation & Linking ────────────────────────────────────────

void OpenGLShader::Compile(std::unordered_map<GLenum, std::string>& shaderSources)
{
    GLuint program = glCreateProgram();

    std::array<GLuint, 6> shaderIDs{};
    uint32_t shaderIndex = 0;

    for (auto& [type, source] : shaderSources)
    {
        GLuint shader = glCreateShader(type);

        const GLchar* sourceCStr = source.c_str();
        DMGE_GL_CALL(glShaderSource(shader, 1, &sourceCStr, nullptr));
        DMGE_GL_CALL(glCompileShader(shader));

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

            std::string infoLog(maxLength, '\0');
            glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());

            DMGE_GL_CALL(glDeleteShader(shader));

            // Clean up previously compiled shaders and the program
            for (uint32_t i = 0; i < shaderIndex; ++i)
                DMGE_GL_CALL(glDeleteShader(shaderIDs[i]));
            DMGE_GL_CALL(glDeleteProgram(program));

            DMGE_LOG_ERROR("{0} shader compilation failed:\n{1}",
                           ShaderTypeName(type), infoLog);
            DMGE_CORE_ASSERT(false, "{0} shader compilation failed!", ShaderTypeName(type));
            return;
        }

        DMGE_GL_CALL(glAttachShader(program, shader));
        shaderIDs[shaderIndex++] = shader;
    }

    DMGE_GL_CALL(glLinkProgram(program));

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        std::string infoLog(maxLength, '\0');
        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());

        DMGE_GL_CALL(glDeleteProgram(program));

        for (uint32_t i = 0; i < shaderIndex; ++i)
            DMGE_GL_CALL(glDeleteShader(shaderIDs[i]));

        DMGE_LOG_ERROR("Shader program linking failed:\n{0}", infoLog);
        DMGE_CORE_ASSERT(false, "Shader program linking failed!");
        return;
    }

    // Detach and delete shader objects (already linked into program)
    for (uint32_t i = 0; i < shaderIndex; ++i)
    {
        DMGE_GL_CALL(glDetachShader(program, shaderIDs[i]));
        DMGE_GL_CALL(glDeleteShader(shaderIDs[i]));
    }

    m_RendererID = program;
}

// ── Uniform Location Cache ───────────────────────────────────────

GLint OpenGLShader::GetUniformLocation(std::string_view name) const
{
    std::string key(name);
    auto it = m_UniformLocationCache.find(key);
    if (it != m_UniformLocationCache.end())
        return it->second;

    GLint location = glGetUniformLocation(m_RendererID, key.c_str());
    DMGE_CORE_ASSERT(location != -1, "Uniform '{0}' not found in shader!", name);

    m_UniformLocationCache[key] = location;
    return location;
}

} // namespace DMGameEngine
