/*
 * DMGameEngine - UniformSerializer (internal shared uniform <-> JSON helpers)
 *
 * INTERNAL HEADER - NOT part of the public API. This file includes
 * nlohmann/json, which is a PRIVATE dependency of the engine, so it must only
 * be included from .cpp translation units compiled into the engine DLL
 * (currently AssetLoader.cpp and SceneSerializer.cpp). Never include it from a
 * public header or from DMGameEngine.h - doing so would leak the json
 * dependency to consumers.
 *
 * Both directions of uniform (de)serialization live here so the type-tag
 * dispatch is written once and reused by:
 *   - AssetLoader<Material>  : reads a .mat file's "uniforms" map
 *   - SceneSerializer        : reads/writes per-instance MaterialOverride
 *                              uniforms on MeshComponent
 *
 * The { "type": <Tag>, "value": ... } shape is identical in .mat files and in
 * .scene materialOverrides, so a Material saved one way round-trips the other.
 *
 * Type tags <-> UniformValue alternatives:
 *   Int / Float / Float2 / Float3 / Float4 / Mat4 / IntArray
 */
#pragma once

#include "DMGameEngine/Renderer/Material.h"   // UniformValue, Material

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace DMGameEngine {

// ── UniformValue (variant) -> JSON { "type": <Tag>, "value": ... } ───────
//
// std::visit + if constexpr dispatch over the closed UniformValue variant.
// Tag names mirror AssetLoader<Material>'s .mat format so the same JSON is
// produced by serialization and consumed by .mat / override loading.
inline nlohmann::json UniformValueToJson(const UniformValue& v)
{
    return std::visit([](auto&& arg) -> nlohmann::json {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) {
            return { {"type", "Int"}, {"value", arg} };
        }
        else if constexpr (std::is_same_v<T, float>) {
            return { {"type", "Float"}, {"value", arg} };
        }
        else if constexpr (std::is_same_v<T, glm::vec2>) {
            return { {"type", "Float2"},
                     {"value", nlohmann::json::array({ arg.x, arg.y })} };
        }
        else if constexpr (std::is_same_v<T, glm::vec3>) {
            return { {"type", "Float3"},
                     {"value", nlohmann::json::array({ arg.x, arg.y, arg.z })} };
        }
        else if constexpr (std::is_same_v<T, glm::vec4>) {
            return { {"type", "Float4"},
                     {"value", nlohmann::json::array({ arg.x, arg.y, arg.z, arg.w })} };
        }
        else if constexpr (std::is_same_v<T, glm::mat4>) {
            // Column-major: 16 floats, index = col*4 + row (matches the reader).
            nlohmann::json arr = nlohmann::json::array();
            for (int i = 0; i < 16; ++i)
                arr.push_back(arg[i / 4][i % 4]);
            return { {"type", "Mat4"}, {"value", std::move(arr)} };
        }
        else if constexpr (std::is_same_v<T, std::vector<int>>) {
            return { {"type", "IntArray"}, {"value", arg} };
        }
        // UniformValue is a closed variant: every alternative is handled above,
        // so every instantiation returns on one branch. Adding a new alternative
        // without a matching branch fails to compile here (compile-time
        // exhaustiveness check) rather than silently dropping the value.
    }, v);
}

// ── JSON { "type": <Tag>, "value": ... } -> Material::Set* by type tag ────
//
// Works for both Material (writes m_Uniforms) and MaterialInstance (the
// virtual Set* overrides write m_Overrides), so the .mat loader and the scene
// override rebuilder share one code path. Unknown type tags and entries
// missing a "value" field are silently skipped (tolerant of user-authored
// files; a skipped uniform simply keeps its default).
inline void ApplyUniform(Material* material, const std::string& name,
                         const nlohmann::json& val)
{
    if (!val.contains("value")) return;
    const std::string type = val.value("type", "");
    const auto& v = val["value"];

    if (type == "Int")
        material->SetInt(name, v.get<int>());
    else if (type == "Float")
        material->SetFloat(name, v.get<float>());
    else if (type == "Float2")
        material->SetFloat2(name, glm::vec2(v[0].get<float>(), v[1].get<float>()));
    else if (type == "Float3")
        material->SetFloat3(name, glm::vec3(v[0].get<float>(), v[1].get<float>(),
                                            v[2].get<float>()));
    else if (type == "Float4")
        material->SetFloat4(name, glm::vec4(v[0].get<float>(), v[1].get<float>(),
                                            v[2].get<float>(), v[3].get<float>()));
    else if (type == "Mat4")
    {
        glm::mat4 m(1.0f);
        for (int i = 0; i < 16; ++i)
            m[i / 4][i % 4] = v[i].get<float>();
        material->SetMat4(name, m);
    }
    else if (type == "IntArray")
    {
        std::vector<int> arr = v.get<std::vector<int>>();
        material->SetIntArray(name, arr.data(), static_cast<uint32_t>(arr.size()));
    }
}

} // namespace DMGameEngine