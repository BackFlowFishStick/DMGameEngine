/*
 * DMGameEngine - Material Implementation
 */

#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Core/Log.h"

#include <utility>

namespace DMGameEngine {

namespace {

// std::visit helper: aggregate of lambdas, exposed as a single callable.
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Upload every uniform in a map to the given shader. Shared by Material
// (its own uniforms) and MaterialInstance (its overrides).
void UploadUniforms(const DM::Ref<Shader>& shader,
                    const std::unordered_map<std::string, UniformValue>& uniforms)
{
    for (const auto& [name, value] : uniforms)
    {
        std::visit(overloaded{
            [&](int v)                     { shader->SetInt(name, v); },
            [&](float v)                   { shader->SetFloat(name, v); },
            [&](const glm::vec2& v)        { shader->SetFloat2(name, v); },
            [&](const glm::vec3& v)        { shader->SetFloat3(name, v); },
            [&](const glm::vec4& v)        { shader->SetFloat4(name, v); },
            [&](const glm::mat4& v)        { shader->SetMat4(name, v); },
            [&](const std::vector<int>& v) { shader->SetIntArray(name, v.data(),
                                              static_cast<uint32_t>(v.size())); }
        }, value);
    }
}

} // anonymous namespace

// ── Material ──────────────────────────────────────────────────────

Material::Material(DM::Ref<Shader> shader)
    : m_Shader(std::move(shader))
{
    DMGE_CORE_ASSERT(m_Shader, "Material - shader is null!");
}

void Material::Bind() const
{
    DMGE_CORE_ASSERT(m_Shader, "Material::Bind - shader is null!");
    m_Shader->Bind();
    UploadUniforms(m_Shader, m_Uniforms);
}

// ── Material Uniform Setters ───────────────────────────────────────

void Material::SetInt(std::string_view name, int value)
{
    m_Uniforms[std::string(name)] = value;
}

void Material::SetIntArray(std::string_view name, const int* values, uint32_t count)
{
    m_Uniforms[std::string(name)] = std::vector<int>(values, values + count);
}

void Material::SetFloat(std::string_view name, float value)
{
    m_Uniforms[std::string(name)] = value;
}

void Material::SetFloat2(std::string_view name, const glm::vec2& value)
{
    m_Uniforms[std::string(name)] = value;
}

void Material::SetFloat3(std::string_view name, const glm::vec3& value)
{
    m_Uniforms[std::string(name)] = value;
}

void Material::SetFloat4(std::string_view name, const glm::vec4& value)
{
    m_Uniforms[std::string(name)] = value;
}

void Material::SetMat4(std::string_view name, const glm::mat4& value)
{
    m_Uniforms[std::string(name)] = value;
}

// ── Material Uniform Queries ──────────────────────────────────────

bool Material::Has(std::string_view name) const
{
    return m_Uniforms.find(std::string(name)) != m_Uniforms.end();
}

const UniformValue* Material::Get(std::string_view name) const
{
    auto it = m_Uniforms.find(std::string(name));
    return it != m_Uniforms.end() ? &it->second : nullptr;
}

// ── MaterialInstance ──────────────────────────────────────────────

MaterialInstance::MaterialInstance(DM::Ref<Material> baseMaterial)
    : Material(baseMaterial ? baseMaterial->GetShader() : nullptr)
    , m_BaseMaterial(std::move(baseMaterial))
{
    DMGE_CORE_ASSERT(m_BaseMaterial, "MaterialInstance - base material is null!");
}

void MaterialInstance::Bind() const
{
    DMGE_CORE_ASSERT(m_BaseMaterial, "MaterialInstance::Bind - base material is null!");
    // Base first: binds the shader and uploads the shared default uniforms.
    m_BaseMaterial->Bind();
    // Then layer this instance's overrides on top (siblings stay unaffected).
    UploadUniforms(GetShader(), m_Overrides);
}

// ── MaterialInstance Override Setters ──────────────────────────────

void MaterialInstance::SetInt(std::string_view name, int value)
{
    m_Overrides[std::string(name)] = value;
}

void MaterialInstance::SetIntArray(std::string_view name, const int* values, uint32_t count)
{
    m_Overrides[std::string(name)] = std::vector<int>(values, values + count);
}

void MaterialInstance::SetFloat(std::string_view name, float value)
{
    m_Overrides[std::string(name)] = value;
}

void MaterialInstance::SetFloat2(std::string_view name, const glm::vec2& value)
{
    m_Overrides[std::string(name)] = value;
}

void MaterialInstance::SetFloat3(std::string_view name, const glm::vec3& value)
{
    m_Overrides[std::string(name)] = value;
}

void MaterialInstance::SetFloat4(std::string_view name, const glm::vec4& value)
{
    m_Overrides[std::string(name)] = value;
}

void MaterialInstance::SetMat4(std::string_view name, const glm::mat4& value)
{
    m_Overrides[std::string(name)] = value;
}

// ── MaterialInstance Uniform Queries ──────────────────────────────

bool MaterialInstance::Has(std::string_view name) const
{
    if (m_Overrides.find(std::string(name)) != m_Overrides.end())
        return true;
    return m_BaseMaterial->Has(name);
}

const UniformValue* MaterialInstance::Get(std::string_view name) const
{
    auto it = m_Overrides.find(std::string(name));
    if (it != m_Overrides.end())
        return &it->second;
    return m_BaseMaterial->Get(name);
}

} // namespace DMGameEngine