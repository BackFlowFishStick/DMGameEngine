/*
 * DMGameEngine - Asset types (stage 1a UUID version)
 *
 * AssetUUID: stable 64-bit identity for a resource (decoupled from its file
 * path, which lives in AssetMetadata and can change). AssetMetadata holds the
 * path + type + dependency graph. AssetTypeOf<T> maps a C++ resource type to
 * its AssetType for type-checking in AssetManager::Load.
 *
 * See ASSET_UUID_CONCEPTS.md part 3/4 for the design.
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace DMGameEngine {

using AssetUUID = uint64_t;
constexpr AssetUUID NullUUID = 0;

enum class AssetType : uint8_t
{
    None = 0,
    Shader,
    Texture2D,
    TextureCube,
    Texture2DArray,
    Material,
    Mesh,
    Audio,
    Skeleton,       // animation: joint hierarchy + inverse binds (DMGE_ANIMATION)
    AnimationClip   // animation: keyframe channels (DMGE_ANIMATION)
};

struct AssetMetadata
{
    AssetUUID              UUID = NullUUID;
    std::string            Path;          // current file path (mutable)
    AssetType              Type = AssetType::None;
    std::vector<AssetUUID> Dependencies;  // e.g. Material -> Shader + Textures
};

// T -> AssetType. Specialize per resource type (see below).
template<typename T>
inline AssetType AssetTypeOf() { return AssetType::None; }

// Forward declarations so specializations compile without pulling the full headers.
class Shader;
class Texture2D;
class Material;
class Mesh;
class Skeleton;
class AnimationClip;

template<> inline AssetType AssetTypeOf<Shader>()    { return AssetType::Shader; }
template<> inline AssetType AssetTypeOf<Texture2D>()  { return AssetType::Texture2D; }
template<> inline AssetType AssetTypeOf<Material>()   { return AssetType::Material; }
template<> inline AssetType AssetTypeOf<Mesh>()       { return AssetType::Mesh; }
template<> inline AssetType AssetTypeOf<Skeleton>()   { return AssetType::Skeleton; }
template<> inline AssetType AssetTypeOf<AnimationClip>() { return AssetType::AnimationClip; }

} // namespace DMGameEngine