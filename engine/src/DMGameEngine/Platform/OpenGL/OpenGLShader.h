/*
 * DMGameEngine - OpenGL Shader
 *
 * OpenGL implementation of the Shader abstraction.
 * Compiles GLSL vertex/fragment shaders and manages uniform uploads.
 */

#pragma once

#include "DMGameEngine/Renderer/Shader.h"

#include <glad/glad.h>

#include <string>
#include <unordered_map>
#include <string_view>

namespace DMGameEngine {

class DMGE_API OpenGLShader : public Shader
{
public:
    OpenGLShader(std::string_view name,
                 std::string_view vertexSrc,
                 std::string_view fragmentSrc);
    explicit OpenGLShader(std::string_view filepath);
    ~OpenGLShader() override;

    void Bind()   const override;
    void Unbind() const override;

    const std::string& GetName() const override { return m_Name; }

    // ── Uniform setters ─────────────────────────────────────────
    void SetInt(std::string_view name, int value) override;
    void SetIntArray(std::string_view name, const int* values, uint32_t count) override;
    void SetFloat(std::string_view name, float value) override;
    void SetFloat2(std::string_view name, const glm::vec2& value) override;
    void SetFloat3(std::string_view name, const glm::vec3& value) override;
    void SetFloat4(std::string_view name, const glm::vec4& value) override;
    void SetMat4(std::string_view name, const glm::mat4& value) override;

private:
    std::string ReadFile(std::string_view filepath) const;
    std::unordered_map<GLenum, std::string> PreProcess(std::string_view source) const;
    void Compile(std::unordered_map<GLenum, std::string>& shaderSources);

    GLint GetUniformLocation(std::string_view name) const;

    uint32_t m_RendererID = 0;
    std::string m_Name;
    std::string m_FilePath;

    mutable std::unordered_map<std::string, GLint> m_UniformLocationCache;
};

} // namespace DMGameEngine
