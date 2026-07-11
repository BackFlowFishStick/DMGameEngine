/*
 * DMGameEngine - Material
 *
 * A Material bundles a Shader with the uniform parameter values that
 * should be applied to it. Rather than uploading uniforms ad-hoc each
 * frame, callers set named values on a Material once; Bind() then binds
 * the shader and uploads every stored uniform in a single pass.
 *
 * MaterialInstance derives from Material: it references a shared base
 * Material (shader + default uniforms) and layers its own overridden
 * uniform values on top at Bind() time, so many instances sharing one
 * Material can each tweak parameters without affecting one another.
 *
 * Material / MaterialInstance are backend-agnostic: they operate purely
 * through the Shader abstraction, so no platform-specific factory or
 * subclass is needed.
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
// A type-erased container for a single uniform value. Each alternative
// corresponds to one of the Shader uniform setters. Integer arrays are
// stored as std::vector<int> so the owner keeps a stable copy that can
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
    virtual ~Material() = default;

    // Binds the shader and uploads every stored uniform value.
    // Virtual so MaterialInstance can layer overrides on top of the base.
    virtual void Bind() const;

    const std::shared_ptr<Shader>& GetShader() const { return m_Shader; }

    // ── Uniform setters (mirror the Shader API by name) ────────
    virtual void SetInt(std::string_view name, int value);
    virtual void SetIntArray(std::string_view name, const int* values, uint32_t count);
    virtual void SetFloat(std::string_view name, float value);
    virtual void SetFloat2(std::string_view name, const glm::vec2& value);
    virtual void SetFloat3(std::string_view name, const glm::vec3& value);
    virtual void SetFloat4(std::string_view name, const glm::vec4& value);
    virtual void SetMat4(std::string_view name, const glm::mat4& value);

    // ── Uniform queries ─────────────────────────────────────────
    virtual bool Has(std::string_view name) const;
    virtual const UniformValue* Get(std::string_view name) const;

private:
    std::shared_ptr<Shader>                       m_Shader;
    std::unordered_map<std::string, UniformValue> m_Uniforms;
};

// ── MaterialInstance ──────────────────────────────────────────────
//
// Shares a base Material (its Shader + default uniforms) but keeps a
// private override map. Bind() applies the base first, then the
// overridden uniforms of this instance on top, so sibling instances do
// not affect one another. Because it derives from Material it can be
// passed to Renderer::Submit(const std::shared_ptr<Material>&, ...).

class DMGE_API MaterialInstance : public Material
{
public:
    explicit MaterialInstance(std::shared_ptr<Material> baseMaterial);

    // Binds the base material (shader + base uniforms), then uploads
    // this instance's overridden uniforms on top of them.
    void Bind() const override;

    const std::shared_ptr<Material>& GetBaseMaterial() const { return m_BaseMaterial; }

    // ── Uniform overrides (stored locally, never touch the base) ─
    void SetInt(std::string_view name, int value) override;
    void SetIntArray(std::string_view name, const int* values, uint32_t count) override;
    void SetFloat(std::string_view name, float value) override;
    void SetFloat2(std::string_view name, const glm::vec2& value) override;
    void SetFloat3(std::string_view name, const glm::vec3& value) override;
    void SetFloat4(std::string_view name, const glm::vec4& value) override;
    void SetMat4(std::string_view name, const glm::mat4& value) override;

    // ── Uniform queries (override first, then base) ─────────────
    bool Has(std::string_view name) const override;
    const UniformValue* Get(std::string_view name) const override;

private:
    std::shared_ptr<Material>                       m_BaseMaterial;
    std::unordered_map<std::string, UniformValue>   m_Overrides;
};

} // namespace DMGameEngine