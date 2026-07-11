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

} // anonymous namespace

// ── Construction ──────────────────────────────────────────────────

Material::Material(std::shared_ptr<Shader> shader)
    : m_Shader(std::move(shader))
{
    DMGE_CORE_ASSERT(m_Shader, "Material - shader is null!");
}

// ── Bind ──────────────────────────────────────────────────────────

void Material::Bind() const
{
    DMGE_CORE_ASSERT(m_Shader, "Material::Bind - shader is null!");
    m_Shader->Bind();

    for (const auto& [name, value] : m_Uniforms)
    {
        std::visit(overloaded{
            [&](int v)                     { m_Shader->SetInt(name, v); },
            [&](float v)                   { m_Shader->SetFloat(name, v); },
            [&](const glm::vec2& v)        { m_Shader->SetFloat2(name, v); },
            [&](const glm::vec3& v)        { m_Shader->SetFloat3(name, v); },
            [&](const glm::vec4& v)        { m_Shader->SetFloat4(name, v); },
            [&](const glm::mat4& v)        { m_Shader->SetMat4(name, v); },
            [&](const std::vector<int>& v) { m_Shader->SetIntArray(name, v.data(),
                                              static_cast<uint32_t>(v.size())); }
        }, value);
    }
}

// ── Uniform Setters ───────────────────────────────────────────────

void Material::SetInt(std::string_view name, int value)
{
    m_Uniforms[std::string(name)] = value;
}

void Material::SetIntArray(std::string_view name, const int* values, uint32_t count)
{
    m_Uniforms[std::string(name)] =
        std::vector<int>(values, values + count);
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

// ── Uniform Queries ──────────────────────────────────────────────

bool Material::Has(std::string_view name) const
{
    return m_Uniforms.find(std::string(name)) != m_Uniforms.end();
}

const UniformValue* Material::Get(std::string_view name) const
{
    auto it = m_Uniforms.find(std::string(name));
    return it != m_Uniforms.end() ? &it->second : nullptr;
}

} // namespace DMGameEngine
