/*
 * DMGameEngine - assimp mesh import (FBX/OBJ/GLTF/GLB -> Mesh)
 *
 * INTERNAL translation unit - not part of the public API. Localizes assimp
 * includes here so AssetLoader.cpp stays assimp-free. Exposes one entry point
 * used by AssetLoader<Mesh>::Load's extension dispatch:
 *   DM::Ref<Mesh> LoadMeshViaAssimp(const std::string& path);
 * and (under DMGE_ANIMATION) the skeleton/clip entry points used by the
 * Animation loaders:
 *   DM::Ref<Skeleton>      LoadSkeletonViaAssimp(modelPath);
 *   DM::Ref<AnimationClip> LoadAnimationClipViaAssimp(modelPath, clipIndex);
 *
 * Pipeline: assimp Importer -> aiScene -> recursive node walk (baking global
 * transforms) -> flatten all aiMeshes into one Mesh (one SubMesh per aiMesh,
 * each an index range into the shared VB/IB). The vertex layout is the superset
 * of channels present across all aiMeshes (Position always; Normal/TexCoords/
 * Tangent when any mesh has them); absent channels in a mesh are zero-filled so
 * the stride stays uniform.
 *
 * Skinned path (DMGE_ANIMATION): when any aiMesh carries aiBones, the layout
 * grows Float4 a_BoneIndices + Float4 a_BoneWeights (top-4 weights, normalized;
 * indices stored as float so the interleaved vertex buffer stays float-only),
 * vertices are written in MESH-LOCAL space (no node baking - stage-1 assumes
 * the mesh node's global transform is identity, true for standard skinned
 * models), and the Skeleton + AnimationClips are registered into AssetManager
 * under derived paths "<model>#skeleton" / "<model>#anim/<i>" (identity/path
 * separation per KB-05; the data loads lazily through the loaders).
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

#ifdef DMGE_ANIMATION
#include "DMGameEngine/Animation/Skeleton.h"
#include "DMGameEngine/Animation/AnimationClip.h"
#endif

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "glm/glm.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace DMGameEngine {

namespace {

// Per-vertex bone influence (top-4 after reduction). Always-compiled tiny POD
// so the vertex loop below needs no ifdef; used only when bones are present.
struct BoneWeight
{
    int32_t JointIndex = -1;
    float   Weight     = 0.0f;
};
using VertexBoneWeights = std::vector<std::vector<BoneWeight>>;   // [vertex] -> influences

// Superset of vertex channels present across all aiMeshes, so a single
// BufferLayout (uniform stride) can describe the flattened Mesh.
struct MeshChannels
{
    bool HasNormal    = false;
    bool HasTexCoords = false;
    bool HasTangent   = false;
    bool HasBones     = false;
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
#ifdef DMGE_ANIMATION
        if (mesh->mNumBones > 0)     ch.HasBones     = true;
#endif
    }
    return ch;
}

// Builds Mesh::Layout from the detected channels (Position first, then the
// optional channels in a fixed order so the stride/offsets are deterministic).
// Bone attributes always come last (indices as Float4 so the interleaved
// vertex buffer stays float-only; the skinned shader casts to ivec4).
void BuildLayout(Mesh& out, const MeshChannels& ch)
{
    out.Layout.AddElement(BufferElement(ShaderDataType::Float3, "a_Position"));
    if (ch.HasNormal)    out.Layout.AddElement(BufferElement(ShaderDataType::Float3, "a_Normal"));
    if (ch.HasTexCoords) out.Layout.AddElement(BufferElement(ShaderDataType::Float2, "a_TexCoords"));
    if (ch.HasTangent)   out.Layout.AddElement(BufferElement(ShaderDataType::Float3, "a_Tangent"));
#ifdef DMGE_ANIMATION
    if (ch.HasBones)
    {
        out.Layout.AddElement(BufferElement(ShaderDataType::Float4, "a_BoneIndices"));
        out.Layout.AddElement(BufferElement(ShaderDataType::Float4, "a_BoneWeights"));
    }
#endif
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

#ifdef DMGE_ANIMATION
// ── Skinned import machinery (DMGE_ANIMATION) ───────────────────────

// Raw joints in first-encounter order + name -> raw index. Inverse bind from
// the first aiBone seen for a name (assimp duplicates bones per mesh).
void CollectJointsRaw(const aiScene* scene, std::vector<Joint>& outJoints,
                      std::unordered_map<std::string, int32_t>& outNameToRaw)
{
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        for (unsigned int b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            const std::string name = bone->mName.C_Str();
            if (outNameToRaw.count(name)) continue;
            outNameToRaw[name] = static_cast<int32_t>(outJoints.size());

            Joint joint;
            joint.Name = name;
            joint.InverseBindMatrix = ToGlm(bone->mOffsetMatrix);
            outJoints.push_back(joint);
        }
    }
}

// Resolve a joint's parent index by walking the node hierarchy up from the
// bone node to the nearest ancestor that is itself a joint (-1 = root).
int32_t ResolveParentIndex(const aiScene* scene, const char* jointName,
                           const std::unordered_map<std::string, int32_t>& nameToRaw)
{
    const aiNode* node = scene->mRootNode->FindNode(jointName);
    if (!node) return -1;
    for (node = node->mParent; node; node = node->mParent)
    {
        const auto it = nameToRaw.find(node->mName.C_Str());
        if (it != nameToRaw.end())
            return it->second;
    }
    return -1;
}

// Reorder joints so every parent precedes its children (AnimationMath's
// forward-pass invariant) and produce the final name -> index map.
void ReorderParentsFirst(const aiScene* scene, std::vector<Joint>& joints,
                         std::unordered_map<std::string, int32_t>& nameToRaw,
                         std::unordered_map<std::string, int32_t>& outNameToFinal)
{
    const size_t n = joints.size();

    // depth per raw joint (walk the raw parent chain).
    std::vector<int32_t> parentOf(n);
    std::vector<size_t>  depth(n);
    for (size_t i = 0; i < n; ++i)
    {
        parentOf[i] = ResolveParentIndex(scene, joints[i].Name.c_str(), nameToRaw);
        size_t d = 0;
        for (int32_t p = parentOf[i]; p >= 0; p = parentOf[static_cast<size_t>(p)])
            ++d;
        depth[i] = d;
    }

    // Order by (depth, original index) so relative order is stable.
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return depth[a] < depth[b]; });

    std::vector<int32_t> newIndexOf(n, -1);
    std::vector<Joint>   ordered;
    ordered.reserve(n);
    for (size_t rank = 0; rank < n; ++rank)
    {
        const size_t raw = order[rank];
        newIndexOf[raw] = static_cast<int32_t>(rank);
        ordered.push_back(joints[raw]);
    }
    for (size_t rank = 0; rank < n; ++rank)
    {
        const size_t raw = order[rank];
        const int32_t rawParent = parentOf[raw];
        ordered[rank].ParentIndex =
            rawParent >= 0 ? newIndexOf[static_cast<size_t>(rawParent)] : -1;
        outNameToFinal[ordered[rank].Name] = static_cast<int32_t>(rank);
    }

    joints = std::move(ordered);
}

// Per-aiMesh per-vertex influences, joint indices resolved through the FINAL
// name -> index map (joint names are unique; ordering is already final).
void CollectVertexWeights(const aiScene* scene,
                          const std::unordered_map<std::string, int32_t>& nameToFinal,
                          std::vector<VertexBoneWeights>& outMeshWeights)
{
    outMeshWeights.assign(scene->mNumMeshes, {});
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        auto& weights = outMeshWeights[m];
        weights.assign(mesh->mNumVertices, {});

        for (unsigned int b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            const auto it = nameToFinal.find(bone->mName.C_Str());
            if (it == nameToFinal.end()) continue;   // not a registered joint

            const int32_t jointIdx = it->second;
            for (unsigned int w = 0; w < bone->mNumWeights; ++w)
            {
                const aiVertexWeight& vw = bone->mWeights[w];
                if (vw.mWeight <= 0.0f) continue;
                if (vw.mVertexId >= mesh->mNumVertices) continue;   // defensive
                weights[vw.mVertexId].push_back({jointIdx, vw.mWeight});
            }
        }
    }
}

// Reduce a vertex's influences to the top-4 (highest weight, ties broken by
// first encounter) and normalize so the weights sum to 1.
void ReduceWeights(std::vector<BoneWeight>& influences)
{
    if (influences.size() > 4)
    {
        std::stable_sort(influences.begin(), influences.end(),
                         [](const BoneWeight& a, const BoneWeight& b)
                         { return a.Weight > b.Weight; });
        influences.resize(4);
    }
    float sum = 0.0f;
    for (const auto& w : influences) sum += w.Weight;
    if (sum > 0.0f)
        for (auto& w : influences) w.Weight /= sum;
}

// AnimationClips from all aiAnimations. Channels targeting nodes that are not
// joints are skipped (stage-1 limitation: no node animation without a joint).
std::vector<AnimationClip> BuildClips(
    const aiScene* scene,
    const std::unordered_map<std::string, int32_t>& nameToFinal)
{
    std::vector<AnimationClip> clips;
    for (unsigned int a = 0; a < scene->mNumAnimations; ++a)
    {
        const aiAnimation* anim = scene->mAnimations[a];
        AnimationClip clip;
        clip.Name           = anim->mName.C_Str();
        clip.Duration       = static_cast<float>(anim->mDuration);
        clip.TicksPerSecond = anim->mTicksPerSecond > 0.0
                            ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;

        for (unsigned int c = 0; c < anim->mNumChannels; ++c)
        {
            const aiNodeAnim* na = anim->mChannels[c];
            const auto it = nameToFinal.find(na->mNodeName.C_Str());
            if (it == nameToFinal.end()) continue;

            AnimationChannel channel;
            channel.JointIndex = it->second;
            for (unsigned int k = 0; k < na->mNumPositionKeys; ++k)
            {
                const auto& key = na->mPositionKeys[k];
                channel.Translations.push_back(
                    { static_cast<float>(key.mTime),
                      glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
            }
            for (unsigned int k = 0; k < na->mNumRotationKeys; ++k)
            {
                const auto& key = na->mRotationKeys[k];
                const aiQuaternion& q = key.mValue;
                channel.Rotations.push_back(
                    { static_cast<float>(key.mTime),
                      glm::quat(q.w, q.x, q.y, q.z) });
            }
            for (unsigned int k = 0; k < na->mNumScalingKeys; ++k)
            {
                const auto& key = na->mScalingKeys[k];
                channel.Scales.push_back(
                    { static_cast<float>(key.mTime),
                      glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
            }
            clip.Channels.push_back(std::move(channel));
        }
        clips.push_back(std::move(clip));
    }
    return clips;
}

// Shared import prologue: assimp flags + ReadFile + skeleton reconstruction
// (joints ordered parents-first). Returns nullptr on import failure.
const aiScene* ReadSceneForAnimation(const std::string& path,
                                     Assimp::Importer& importer)
{
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
    return scene;
}

void BuildSkeletonFromScene(const aiScene* scene, Skeleton& outSkeleton,
                            std::unordered_map<std::string, int32_t>& outNameToFinal)
{
    std::vector<Joint> joints;
    std::unordered_map<std::string, int32_t> nameToRaw;
    CollectJointsRaw(scene, joints, nameToRaw);
    ReorderParentsFirst(scene, joints, nameToRaw, outNameToFinal);
    outSkeleton.Joints = std::move(joints);
}

#endif // DMGE_ANIMATION

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

// Recursive node walk: bake the accumulated transform (static path), append
// each aiMesh's vertices/indices into the shared Mesh buffers, and emit one
// SubMesh per aiMesh with its material AssetHandle.
//
// Skinned meshes (DMGE_ANIMATION, meshWeights non-null): vertices are written
// in mesh-local space (bakeTransforms=false - the skinning palette is defined
// against the bind pose) and each vertex gets top-4 bone indices/weights.
void ProcessNode(const aiScene* scene, aiNode* node, const glm::mat4& parentTransform,
                 Mesh& out, const MeshChannels& ch,
                 const std::filesystem::path& modelDir, const std::string& modelStem,
                 bool bakeTransforms,
                 const std::vector<VertexBoneWeights>* meshWeights)
{
    const glm::mat4 nodeTransform = bakeTransforms
        ? parentTransform * ToGlm(node->mTransformation)
        : glm::mat4(1.0f);
    const uint32_t strideFloats = out.Layout.GetStride() / static_cast<uint32_t>(sizeof(float));

    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        const unsigned int meshIndex = node->mMeshes[i];
        const aiMesh* aimesh = scene->mMeshes[meshIndex];
        const VertexBoneWeights* vertexWeights =
            meshWeights ? &(*meshWeights)[meshIndex] : nullptr;

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
#ifdef DMGE_ANIMATION
            if (ch.HasBones)
            {
                // Top-4 influences, normalized; zero-fill below 4 so the stride
                // stays uniform. Indices stored as float (skinned shader casts).
                float indices[4]  = {0.0f, 0.0f, 0.0f, 0.0f};
                float weights[4]  = {0.0f, 0.0f, 0.0f, 0.0f};
                if (vertexWeights && v < vertexWeights->size())
                {
                    std::vector<BoneWeight> infl = (*vertexWeights)[v];
                    ReduceWeights(infl);
                    for (size_t k = 0; k < infl.size() && k < 4; ++k)
                    {
                        indices[k] = static_cast<float>(infl[k].JointIndex);
                        weights[k] = infl[k].Weight;
                    }
                }
                for (float idx : indices) out.Vertices.push_back(idx);
                for (float w   : weights) out.Vertices.push_back(w);
            }
#endif
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
        ProcessNode(scene, node->mChildren[c], nodeTransform, out, ch, modelDir,
                    modelStem, bakeTransforms, meshWeights);
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

#ifdef DMGE_ANIMATION
    // Skinned import: reconstruct the Skeleton (joints parents-first), gather
    // per-vertex influences, and register the derived Skeleton/Clip asset
    // paths. Meshes keep their bind-pose vertices (no node baking).
    bool              hasBones     = false;
    Skeleton          skeleton;
    std::vector<AnimationClip> clips;
    std::vector<VertexBoneWeights> meshWeights;
    if (ch.HasBones)
    {
        std::unordered_map<std::string, int32_t> nameToFinal;
        BuildSkeletonFromScene(scene, skeleton, nameToFinal);
        CollectVertexWeights(scene, nameToFinal, meshWeights);
        clips = BuildClips(scene, nameToFinal);
        hasBones = !skeleton.Joints.empty();
    }
    const bool bakeTransforms = !hasBones;
#else
    const bool bakeTransforms = true;
#endif

    const std::filesystem::path p(path);
    const std::filesystem::path modelDir = p.parent_path();
    const std::string modelStem = p.stem().string();

#ifdef DMGE_ANIMATION
    ProcessNode(scene, scene->mRootNode, glm::mat4(1.0f), *mesh, ch, modelDir,
                modelStem, bakeTransforms, hasBones ? &meshWeights : nullptr);
#else
    ProcessNode(scene, scene->mRootNode, glm::mat4(1.0f), *mesh, ch, modelDir,
                modelStem, bakeTransforms, nullptr);
#endif

    if (mesh->Vertices.empty() || mesh->SubMeshes.empty())
    {
        DMGE_LOG_ERROR("Assimp: no geometry imported from '{}'", path);
        return nullptr;
    }

#ifdef DMGE_ANIMATION
    if (hasBones)
    {
        // Register derived paths only - identity/path separation (KB-05); the
        // data loads lazily via AssetLoader<Skeleton>/<AnimationClip>.
        auto& am = AssetManager::Get();
        am.Register(path + "#skeleton", AssetType::Skeleton);
        for (size_t c = 0; c < clips.size(); ++c)
            am.Register(path + "#anim/" + std::to_string(c), AssetType::AnimationClip);
        DMGE_LOG_INFO("Assimp: '{}' imported with skeleton ({} joints) and {} clip(s)",
                      path, skeleton.Joints.size(), clips.size());
    }
#endif
    return mesh;
}

#ifdef DMGE_ANIMATION
// ── Skeleton / clip entry points (used by AnimationAssetLoaders.cpp) ──

DM::Ref<Skeleton> LoadSkeletonViaAssimp(const std::string& modelPath)
{
    Assimp::Importer importer;
    const aiScene* scene = ReadSceneForAnimation(modelPath, importer);
    if (!scene) return nullptr;

    auto skeleton = DM::CreateRef<Skeleton>();
    std::unordered_map<std::string, int32_t> nameToFinal;
    BuildSkeletonFromScene(scene, *skeleton, nameToFinal);
    if (skeleton->Joints.empty())
    {
        DMGE_LOG_WARN("Assimp: no bones in '{}' for skeleton import", modelPath);
        return nullptr;
    }
    return skeleton;
}

DM::Ref<AnimationClip> LoadAnimationClipViaAssimp(const std::string& modelPath,
                                                  uint32_t clipIndex)
{
    Assimp::Importer importer;
    const aiScene* scene = ReadSceneForAnimation(modelPath, importer);
    if (!scene) return nullptr;

    Skeleton skeleton;   // joints needed only for the name -> index mapping
    std::unordered_map<std::string, int32_t> nameToFinal;
    BuildSkeletonFromScene(scene, skeleton, nameToFinal);

    auto clips = BuildClips(scene, nameToFinal);
    if (clipIndex >= clips.size())
    {
        DMGE_LOG_WARN("Assimp: clip index {} out of range ({}) for '{}'",
                      clipIndex, clips.size(), modelPath);
        return nullptr;
    }
    return DM::CreateRef<AnimationClip>(std::move(clips[clipIndex]));
}
#endif // DMGE_ANIMATION

} // namespace DMGameEngine
