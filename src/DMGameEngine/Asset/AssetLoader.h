/*
 * DMGameEngine - AssetLoader<T> (stage 1a minimal)
 *
 * Type-specific resource loading: path -> Ref<T>. Specialize per resource
 * type. The minimal version reuses existing factories (Shader::Create,
 * Texture2D::Create). Add a Material specialization once the .mat format
 * is defined (planned with 1c serialization).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Texture2D.h"
#include <string>

namespace DMGameEngine {

// Primary template - must be specialized per resource type.
template<typename T>
struct AssetLoader;

template<>
struct AssetLoader<Shader>
{
    // Reads the shader file (type-tagged GLSL) and compiles via the active backend.
    static DM::Ref<Shader> Load(const std::string& path)
    {
        return Shader::Create(path);
    }
};

template<>
struct AssetLoader<Texture2D>
{
    // Reads an image file and uploads it via the active backend.
    static DM::Ref<Texture2D> Load(const std::string& path)
    {
        return Texture2D::Create(path);
    }
};

} // namespace DMGameEngine