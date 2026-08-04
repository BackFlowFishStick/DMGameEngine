/*
 * DMGameEngine - assimp mesh import (FBX/OBJ/GLTF/GLB -> Mesh)
 *
 * INTERNAL translation unit - not part of the public API. Localizes assimp
 * includes here so AssetLoader.cpp stays assimp-free. Exposes one entry point
 * used by AssetLoader<Mesh>::Load's extension dispatch:
 *   DM::Ref<Mesh> LoadMeshViaAssimp(const std::string& path);
 *
 * Pipeline: assimp Importer -> aiScene -> recursive node walk (baking global
 * transforms) -> flatten all aiMeshes into one Mesh (one SubMesh per aiMesh,
 * each an index range into the shared VB/IB). The vertex layout is the superset
 * of channels present across all aiMeshes (Position always; Normal/TexCoords/
 * Tangent when any mesh has them); absent channels in a mesh are zero-filled so
 * the stride stays uniform.
 *
 * Material mapping: per aiMaterial, look for a sibling .mat file by material
 * name (<modelDir>/<materialName>.mat) and Register it as a Material asset
 * (AssetHandle). Missing .mat leaves SubMesh.MaterialAsset invalid (render
 * skips that draw until a .mat is authored). No texture binding (Material has
 * none yet).
 */
#include "DMGameEngine/Asset/Mesh.h"
#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Asset/AssetTypes.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "glm/glm.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

namespace DMGameEngine {

namespace {

// Superset of vertex channels present across all aiMeshes, so a single
// BufferLayout (uniform stride) can describe the flattened Mesh.
struct MeshChannels
{
    bool HasNormal    = false;
    bool HasTexCoords = false;
    bool HasTangent   = false;
};

MeshChannels DetectChannels(const aiScene* scene)
{
    MeshChannels ch;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        if (mesh->mNormals)          ch.HasNormal    = true;
        if (mesh->mTextureCoords[0]) ch.HasTexCoords = true;
        if (mesh->mTangents)         ch.HasTangent   = true;
    }
    return ch;
}

// Builds Mesh::Layout from the detected channels (Position first, then the
// optional channels in a fixed order so the stride/offsets are deterministic).
void BuildLayout(Mesh& out, const MeshChannels& ch)
{
    out.Layout.AddElement(BufferElement(ShaderDataType::Float3, "a_Position"));
    if (ch.HasNormal)    out.Layout.AddElement(BufferElement(ShaderDataType::Float3, "a_Normal"));
    if (ch.HasTexCoords) out.Layout.AddElement(BufferElement(ShaderDataType::Float2, "a_TexCoords"));
    if (ch.HasTangent)   out.Layout.AddElement(BufferElement(ShaderDataType::Float3, "a_Tangent"));
}

// aiMatrix4x4 is row-major; glm::mat4 is column-major, so transpose while
// copying. Feeding assimp columns as glm rows yields the correct transform.
glm::mat4 ToGlm(const aiMatrix4x4& m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4);
}

glm::vec3 TransformPoint(const glm::mat4& m, const aiVector3D& p)
{
    return glm::vec3(m * glm::vec4(p.x, p.y, p.z, 1.0f));
}

// Transforms a direction (normal/tangent) by the inverse-transpose of the 3x3
// upper-left of m, then re-normalizes (correct under non-uniform scale).
glm::vec3 TransformDirection(const glm::mat4& m, const aiVector3D& d)
{
    const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(m)));
    return glm::normalize(normalMat * glm::vec3(d.x, d.y, d.z));
}

// Sanitize a material name into a safe file stem; empty/whitespace -> fallback.
std::string SanitizeMaterialName(const char* raw, const std::string& fallback)
{
    std::string name = raw ? raw : "";
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return fallback;
    const auto last = name.find_last_not_of(" \t\r\n");
    name = name.substr(first, last - first + 1);
    if (name.empty()) return fallback;
    // Keep it a single path segment.
    std::replace_if(name.begin(), name.end(),
                    [](char c) { return c == '/' || c == '\\' || c == ':'; }, '_');
    return name;
}

// Resolves <modelDir>/<materialName>.mat if it exists, else an empty string.
std::string FindMaterialMatFile(const std::filesystem::path& modelDir,
                                const aiScene* scene, unsigned int materialIndex,
                                const std::string& modelStem)
{
    const aiMaterial* aiMat = scene->mMaterials[materialIndex];
    aiString aiName;
    aiMat->Get(AI_MATKEY_NAME, aiName);

    const std::string fallback = modelStem + "_" + std::to_string(materialIndex);
    const std::string stem = SanitizeMaterialName(aiName.C_Str(), fallback);

    const std::filesystem::path candidate = modelDir / (stem + ".mat");
    if (std::filesystem::exists(candidate))
        return candidate.string();
    return {};
}

// Recursive node walk: bake the accumulated transform, append each aiMesh's
// vertices/indices into the shared Mesh buffers, and emit one SubMesh per
// aiMesh with its material AssetHandle.
void ProcessNode(const aiScene* scene, aiNode* node, const glm::mat4& parentTransform,
                 Mesh& out, const MeshChannels& ch,
                 const std::filesystem::path& modelDir, const std::string& modelStem)
{
    const glm::mat4 nodeTransform = parentTransform * ToGlm(node->mTransformation);
    const uint32_t strideFloats = out.Layout.GetStride() / static_cast<uint32_t>(sizeof(float));

    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        const unsigned int meshIndex = node->mMeshes[i];
        const aiMesh* aimesh = scene->mMeshes[meshIndex];

        // Offset (in vertices) of this aiMesh's vertices within the shared VB.
        const uint32_t vertexOffset =
            static_cast<uint32_t>(out.Vertices.size() / strideFloats);

        SubMesh sub;
        sub.IndexOffset = static_cast<uint32_t>(out.Indices.size());

        // Material: look up a sibling .mat file and register it (UUID only; the
        // actual Material load is deferred to render time). Missing -> invalid.
        const std::string matFile = FindMaterialMatFile(modelDir, scene,
                                                        aimesh->mMaterialIndex, modelStem);
        if (!matFile.empty())
            sub.MaterialAsset = AssetHandle(
                AssetManager::Get().Register(matFile, AssetType::Material));
        else
            DMGE_LOG_WARN("Assimp: no .mat for material {} of '{}'; SubMesh.MaterialAsset left invalid",
                          aimesh->mMaterialIndex, modelStem);

        // Vertices (interleaved per Layout; zero-fill absent channels).
        for (unsigned int v = 0; v < aimesh->mNumVertices; ++v)
        {
            const glm::vec3 pos = TransformPoint(nodeTransform, aimesh->mVertices[v]);
            out.Vertices.push_back(pos.x);
            out.Vertices.push_back(pos.y);
            out.Vertices.push_back(pos.z);

            if (ch.HasNormal)
            {
                const glm::vec3 n = aimesh->mNormals
                    ? TransformDirection(nodeTransform, aimesh->mNormals[v])
                    : glm::vec3(0.0f);
                out.Vertices.push_back(n.x);
                out.Vertices.push_back(n.y);
                out.Vertices.push_back(n.z);
            }
            if (ch.HasTexCoords)
            {
                if (aimesh->mTextureCoords[0])
                {
                    out.Vertices.push_back(aimesh->mTextureCoords[0][v].x);
                    out.Vertices.push_back(aimesh->mTextureCoords[0][v].y);
                }
                else
                {
                    out.Vertices.push_back(0.0f);
                    out.Vertices.push_back(0.0f);
                }
            }
            if (ch.HasTangent)
            {
                const glm::vec3 t = aimesh->mTangents
                    ? TransformDirection(nodeTransform, aimesh->mTangents[v])
                    : glm::vec3(0.0f);
                out.Vertices.push_back(t.x);
                out.Vertices.push_back(t.y);
                out.Vertices.push_back(t.z);
            }
        }

        // Indices (triangulated => 3 per face; remap into the shared VB).
        for (unsigned int f = 0; f < aimesh->mNumFaces; ++f)
        {
            const aiFace& face = aimesh->mFaces[f];
            for (unsigned int idx = 0; idx < face.mNumIndices; ++idx)
                out.Indices.push_back(static_cast<uint32_t>(face.mIndices[idx]) + vertexOffset);
        }

        sub.IndexCount = static_cast<uint32_t>(out.Indices.size()) - sub.IndexOffset;
        out.SubMeshes.push_back(sub);
    }

    for (unsigned int c = 0; c < node->mNumChildren; ++c)
        ProcessNode(scene, node->mChildren[c], nodeTransform, out, ch, modelDir, modelStem);
}

} // namespace

DM::Ref<Mesh> LoadMeshViaAssimp(const std::string& path)
{
    Assimp::Importer importer;
    // aiProcess_GenSmoothNormals: no-op if normals already exist.
    // aiProcess_CalcTangentSpace: generates tangents for meshes with UVs.
    // No aiProcess_FlipUVs: the GL texture loader already flips vertically.
    const unsigned int flags =
          aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_JoinIdenticalVertices
        | aiProcess_CalcTangentSpace
        | aiProcess_ValidateDataStructure;

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        DMGE_LOG_ERROR("Assimp failed to load '{}': {}", path, importer.GetErrorString());
        return nullptr;
    }

    auto mesh = DM::CreateRef<Mesh>();
    const MeshChannels ch = DetectChannels(scene);
    BuildLayout(*mesh, ch);

    const std::filesystem::path p(path);
    const std::filesystem::path modelDir = p.parent_path();
    const std::string modelStem = p.stem().string();

    ProcessNode(scene, scene->mRootNode, glm::mat4(1.0f), *mesh, ch, modelDir, modelStem);

    if (mesh->Vertices.empty() || mesh->SubMeshes.empty())
    {
        DMGE_LOG_ERROR("Assimp: no geometry imported from '{}'", path);
        return nullptr;
    }
    return mesh;
}

} // namespace DMGameEngine
