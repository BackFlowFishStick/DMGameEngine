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
 * Texture support (lighting stage A): Material also holds named texture
 * slots (sampler name -> Ref<Texture> + unit). Bind() binds each texture
 * to its unit and sets the sampler uniform so the shader can sample it.
 * MaterialInstance overrides texture slots the same way it overrides
 * scalar uniforms.
 *
 * Material / MaterialInstance are backend-agnostic: they operate purely
 * through the Shader + Texture abstractions, so no platform-specific
 * factory or subclass is needed.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Texture.h"
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
using UniformValue = std::variant<
    int,
    float,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    glm::mat4,
    std::vector<int>
>;

// ── Texture Slot ────────────────────────────────────────────────────
// Maps a shader sampler uniform name to a Texture and the texture unit
// (slot) it should be bound to. Bind() calls texture->Bind(slot) then
// shader->SetInt(name, slot) so the shader samples the right unit.
struct DMGE_API TextureSlot
{
    DM::Ref<Texture> Texture;
    uint32_t         Slot = 0;
};

// ── Material ───────────────────────────────────────────────────────

class DMGE_API Material
{
public:
    explicit Material(DM::Ref<Shader> shader);
    virtual ~Material() = default;

    // Binds the shader, uploads every stored uniform value, then binds
    // every stored texture and sets its sampler uniform.
    virtual void Bind() const;

    const DM::Ref<Shader>& GetShader() const { return m_Shader; }

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

    // ── Texture slots ──────────────────────────────────────────
    virtual void SetTexture(std::string_view name, const DM::Ref<Texture>& texture, uint32_t slot = 0);
    virtual bool HasTexture(std::string_view name) const;
    virtual const TextureSlot* GetTexture(std::string_view name) const;
    const std::unordered_map<std::string, TextureSlot>& GetTextures() const { return m_Textures; }

private:
    DM::Ref<Shader>                       m_Shader;
    std::unordered_map<std::string, UniformValue> m_Uniforms;
    std::unordered_map<std::string, TextureSlot>  m_Textures;
};

// ── MaterialInstance ──────────────────────────────────────────────
class DMGE_API MaterialInstance : public Material
{
public:
    explicit MaterialInstance(DM::Ref<Material> baseMaterial);

    void Bind() const override;

    const DM::Ref<Material>& GetBaseMaterial() const { return m_BaseMaterial; }

    const std::unordered_map<std::string, UniformValue>& GetOverrides() const { return m_Overrides; }
    const std::unordered_map<std::string, TextureSlot>& GetTextureOverrides() const { return m_TextureOverrides; }

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

    // ── Texture overrides (stored locally, never touch the base) ─
    void SetTexture(std::string_view name, const DM::Ref<Texture>& texture, uint32_t slot = 0) override;
    bool HasTexture(std::string_view name) const override;
    const TextureSlot* GetTexture(std::string_view name) const override;

private:
    DM::Ref<Material>                       m_BaseMaterial;
    std::unordered_map<std::string, UniformValue> m_Overrides;
    std::unordered_map<std::string, TextureSlot>  m_TextureOverrides;
};

} // namespace DMGameEngine
