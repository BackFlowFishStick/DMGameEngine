/*
 * DMGameEngine - AssetLoader<T> (stage 1a + 1c path A)
 *
 * Type-specific resource loading: path -> Ref<T>. Specialize per resource
 * type. Shader/Texture2D reuse existing factories inline; Material reads a
 * .mat JSON file and is implemented in AssetLoader.cpp (keeps nlohmann/json
 * PRIVATE).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Texture2D.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Asset/Mesh.h"
#ifdef DMGE_ANIMATION
#include "DMGameEngine/Animation/Skeleton.h"
#include "DMGameEngine/Animation/AnimationClip.h"
#endif
#include <string>

namespace DMGameEngine {

// Primary template - must be specialized per resource type.
template<typename T>
struct AssetLoader;

template<>
struct AssetLoader<Shader>
{
    static DM::Ref<Shader> Load(const std::string& path) { return Shader::Create(path); }
};

template<>
struct AssetLoader<Texture2D>
{
    static DM::Ref<Texture2D> Load(const std::string& path) { return Texture2D::Create(path); }
};

// Material: reads .mat JSON (shader path + uniforms + textures). Impl in
// AssetLoader.cpp (uses nlohmann/json). Material has no Create() factory -
// constructed directly with a Shader. .mat supports a "textures" field
// (lighting stage A: sampler name -> texture path + slot).
template<>
struct DMGE_API AssetLoader<Material>
{
    static DM::Ref<Material> Load(const std::string& path);
};
// VertexArray (mesh resource): reads .mesh JSON (layout + vertices + indices)
// -> VertexArray (VB + IB). Implementation in AssetLoader.cpp. Standard
// .obj/.gltf need a parser library (assimp/tinygltf) - follow-up.
template<>
struct DMGE_API AssetLoader<VertexArray>
{
    static DM::Ref<VertexArray> Load(const std::string& path);
};

// Mesh: reads .mesh JSON (layout + vertices + indices + submeshes) -> Mesh
// (CPU data + lazy VertexArray). Implementation in AssetLoader.cpp.
template<>
struct DMGE_API AssetLoader<Mesh>
{
    static DM::Ref<Mesh> Load(const std::string& path);
};

#ifdef DMGE_ANIMATION
// Skeleton / AnimationClip (animation stage 1): dispatch between derived
// assimp paths ("<model>#skeleton", "<model>#anim/<i>") and the minimal
// inline JSON formats (.skel.json / .anim.json). Implementation in
// Animation/AnimationAssetLoaders.cpp. DMGE_API per KB-05 rule 2 (header-only
// consumers like AnimationSystem call Load<Skeleton> across the DLL boundary).
template<>
struct DMGE_API AssetLoader<Skeleton>
{
    static DM::Ref<Skeleton> Load(const std::string& path);
};

template<>
struct DMGE_API AssetLoader<AnimationClip>
{
    static DM::Ref<AnimationClip> Load(const std::string& path);
};
#endif // DMGE_ANIMATION

} // namespace DMGameEngine
