/*
 * DMGameEngine - AssetLoader<Material> implementation (stage 1c path A)
 *
 * Reads a .mat JSON file:
 *   { "shader": "<path>", "uniforms": { "<name>": { "type": "<Type>", "value": ... } } }
 * Loads the shader via AssetManager (dedup), constructs Material(shader), and
 * applies each uniform via the matching Material::Set* by type tag.
 *
 * Uniform type tags map to UniformValue alternatives:
 *   Int / Float / Float2 / Float3 / Float4 / Mat4 / IntArray
 */
#include "DMGameEngine/Asset/AssetLoader.h"
#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/Shader.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>

namespace DMGameEngine {

DM::Ref<Material> AssetLoader<Material>::Load(const std::string& path)
{
    std::ifstream fin(path);
    if (!fin.is_open()) return nullptr;
    nlohmann::json j;
    fin >> j;

    // 1. shader (reuse Shader loader for dedup + cache)
    std::string shaderPath = j.value("shader", "");
    if (shaderPath.empty()) return nullptr;
    auto shader = AssetManager::Get().Load<Shader>(shaderPath);
    if (!shader) return nullptr;

    // 2. construct material (no Create() factory; direct ctor with Shader)
    auto material = DM::CreateRef<Material>(shader);

    // 3. uniforms (type-tagged JSON -> Material::Set*)
    if (j.contains("uniforms"))
    {
        for (auto& [name, val] : j["uniforms"].items())
        {
            const std::string type = val.value("type", "");
            const auto& v = val["value"];
            if (type == "Int")
                material->SetInt(name, v.get<int>());
            else if (type == "Float")
                material->SetFloat(name, v.get<float>());
            else if (type == "Float2")
                material->SetFloat2(name, glm::vec2(v[0], v[1]));
            else if (type == "Float3")
                material->SetFloat3(name, glm::vec3(v[0], v[1], v[2]));
            else if (type == "Float4")
                material->SetFloat4(name, glm::vec4(v[0], v[1], v[2], v[3]));
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
    }
    return material;
}


// ── VertexArray (mesh) ──────────────────────────────────────────
// .mesh JSON format:
//   { "layout": [{ "type": "Float3", "name": "a_Position" }, ...],
//     "vertices": [float...],  "indices": [uint32...] }

namespace {
ShaderDataType ParseShaderDataType(const std::string& s)
{
    if (s == "Float")  return ShaderDataType::Float;
    if (s == "Float2") return ShaderDataType::Float2;
    if (s == "Float3") return ShaderDataType::Float3;
    if (s == "Float4") return ShaderDataType::Float4;
    if (s == "Int")    return ShaderDataType::Int;
    if (s == "Int2")   return ShaderDataType::Int2;
    if (s == "Int3")   return ShaderDataType::Int3;
    if (s == "Int4")   return ShaderDataType::Int4;
    if (s == "Bool")   return ShaderDataType::Bool;
    if (s == "Mat3")   return ShaderDataType::Mat3;
    if (s == "Mat4")   return ShaderDataType::Mat4;
    return ShaderDataType::None;
}
} // namespace

DM::Ref<VertexArray> AssetLoader<VertexArray>::Load(const std::string& path)
{
    std::ifstream fin(path);
    if (!fin.is_open()) return nullptr;
    nlohmann::json j;
    fin >> j;

    // layout (dynamic: .mesh declares its own attribute layout)
    BufferLayout layout;
    if (j.contains("layout"))
    {
        for (const auto& el : j["layout"])
        {
            ShaderDataType type = ParseShaderDataType(el.value("type", ""));
            std::string name = el.value("name", "");
            layout.AddElement(BufferElement(type, name));
        }
    }

    // vertices (flat float array, interleaved per layout)
    std::vector<float> vertices;
    if (j.contains("vertices"))
        vertices = j["vertices"].get<std::vector<float>>();
    if (vertices.empty()) return nullptr;

    // indices (uint32)
    std::vector<uint32_t> indices;
    if (j.contains("indices"))
        indices = j["indices"].get<std::vector<uint32_t>>();

    // build VertexArray
    auto va = VertexArray::Create();
    auto vb = VertexBuffer::Create(vertices.data(),
                                   static_cast<uint32_t>(vertices.size() * sizeof(float)));
    vb->SetLayout(layout);
    va->AddVertexBuffer(vb);
    if (!indices.empty())
    {
        auto ib = IndexBuffer::Create(indices.data(),
                                      static_cast<uint32_t>(indices.size()));
        va->SetIndexBuffer(ib);
    }
    return va;
}


// ── Mesh ───────────────────────────────────────────────────────
// .mesh JSON format (extends the VertexArray one with submeshes):
//   { "layout": [{ "type": "Float3", "name": "a_Position" }, ...],
//     "vertices": [float...],  "indices": [uint32...],
//     "submeshes": [{ "indexOffset": 0, "indexCount": 6, "material": <uuid> }] }
// If "submeshes" is absent, a single submesh covering all indices is assumed.

DM::Ref<Mesh> AssetLoader<Mesh>::Load(const std::string& path)
{
    std::ifstream fin(path);
    if (!fin.is_open()) return nullptr;
    nlohmann::json j;
    fin >> j;

    auto mesh = DM::CreateRef<Mesh>();

    // layout (dynamic via BufferLayout::AddElement)
    if (j.contains("layout"))
    {
        for (const auto& el : j["layout"])
        {
            ShaderDataType type = ParseShaderDataType(el.value("type", ""));
            std::string name = el.value("name", "");
            mesh->Layout.AddElement(BufferElement(type, name));
        }
    }

    // vertices
    if (j.contains("vertices"))
        mesh->Vertices = j["vertices"].get<std::vector<float>>();
    if (mesh->Vertices.empty()) return nullptr;

    // indices
    if (j.contains("indices"))
        mesh->Indices = j["indices"].get<std::vector<uint32_t>>();

    // submeshes (material groups); default = one submesh over all indices
    if (j.contains("submeshes"))
    {
        for (const auto& sm : j["submeshes"])
        {
            SubMesh s;
            s.IndexOffset   = sm.value("indexOffset", 0u);
            s.IndexCount    = sm.value("indexCount",  0u);
            s.MaterialAsset = AssetHandle(sm.value("material", uint64_t(0)));
            mesh->SubMeshes.push_back(s);
        }
    }
    else if (!mesh->Indices.empty())
    {
        SubMesh s;
        s.IndexOffset = 0;
        s.IndexCount  = static_cast<uint32_t>(mesh->Indices.size());
        mesh->SubMeshes.push_back(s);
    }

    return mesh;
}

} // namespace DMGameEngine