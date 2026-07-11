/*
 * DMGameEngine - Material
 *
 * A Material bundles a Shader with the uniform parameter values that
 * should be applied to it. Rather than uploading uniforms ad-hoc each
 * frame, callers set named values on a Material once; Bind() then binds
 * the shader and uploads every stored uniform in a single pass.
 *
 * Material is backend-agnostic: it operates purely through the Shader
 * abstraction, so no platform-specific factory or subclass is needed.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "glm/glm.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace DMGameEngine {

// ── Uniform Value Storage ──────────────────────────────────────────
//
// A type-erased container for a single uniform's value. Each alternative
// corresponds to one of the Shader uniform setters. Integer arrays are
// stored as std::vector<int> so the Material owns a stable copy that can
// be re-uploaded on every Bind() without the caller keeping it alive.

using UniformValue = std::variant<
    int,
    float,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    glm::mat4,
    std::vector<int>
>;

// ── Material ───────────────────────────────────────────────────────

class DMGE_API Material
{
public:
    explicit Material(std::shared_ptr<Shader> shader);

    // Binds the shader and uploads every stored uniform value.
    void Bind() const;

    const std::shared_ptr<Shader>& GetShader() const { return m_Shader; }

    // ── Uniform setters (mirror the Shader API by name) ────────
    void SetInt(std::string_view name, int value);
    void SetIntArray(std::string_view name, const int* values, uint32_t count);
    void SetFloat(std::string_view name, float value);
    void SetFloat2(std::string_view name, const glm::vec2& value);
    void SetFloat3(std::string_view name, const glm::vec3& value);
    void SetFloat4(std::string_view name, const glm::vec4& value);
    void SetMat4(std::string_view name, const glm::mat4& value);

    // ── Uniform queries ─────────────────────────────────────────
    bool Has(std::string_view name) const;
    const UniformValue* Get(std::string_view name) const;

private:
    std::shared_ptr<Shader>                       m_Shader;
    std::unordered_map<std::string, UniformValue> m_Uniforms;
};

} // namespace DMGameEngine
