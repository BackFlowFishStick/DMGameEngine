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

// Material: reads .mat JSON (shader path + uniforms). Implementation in
// AssetLoader.cpp (uses nlohmann/json). Material has no Create() factory -
// constructed directly with a Shader; no texture binding (textures bound
// elsewhere), so .mat has no "textures" field.
template<>
struct AssetLoader<Material>
{
    static DM::Ref<Material> Load(const std::string& path);
};

// VertexArray (mesh resource): reads .mesh JSON (layout + vertices + indices)
// -> VertexArray (VB + IB). Implementation in AssetLoader.cpp. Standard
// .obj/.gltf need a parser library (assimp/tinygltf) - follow-up.
template<>
struct AssetLoader<VertexArray>
{
    static DM::Ref<VertexArray> Load(const std::string& path);
};

// Mesh: reads .mesh JSON (layout + vertices + indices + submeshes) -> Mesh
// (CPU data + lazy VertexArray). Implementation in AssetLoader.cpp.
template<>
struct AssetLoader<Mesh>
{
    static DM::Ref<Mesh> Load(const std::string& path);
};

} // namespace DMGameEngine