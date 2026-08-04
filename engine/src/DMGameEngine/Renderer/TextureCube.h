/*
 * DMGameEngine - TextureCube
 *
 * Cubemap texture abstraction. A cube map is six square faces covering
 * the +X/-X/+Y/-Y/+Z/-Z axes. Derives from Texture and adds cube-specific
 * specification, per-face data upload and a factory.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Texture.h"

#include <array>
#include <cstdint>
#include <string>

namespace DMGameEngine {

// -- Cube Face -----------------------------------------------------
//
// Canonical OpenGL cube-map face order. The integer value matches the
// offset added to GL_TEXTURE_CUBE_MAP_POSITIVE_X, so it can be used
// directly as a face index.

enum class CubeFace : uint8_t
{
    Right  = 0,  // +X  GL_TEXTURE_CUBE_MAP_POSITIVE_X
    Left   = 1,  // -X  GL_TEXTURE_CUBE_MAP_NEGATIVE_X
    Top    = 2,  // +Y  GL_TEXTURE_CUBE_MAP_POSITIVE_Y
    Bottom = 3,  // -Y  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
    Front  = 4,  // +Z  GL_TEXTURE_CUBE_MAP_POSITIVE_Z
    Back   = 5   // -Z  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

static constexpr uint32_t CubeFaceCount = 6;

// -- TextureCube Specification -------------------------------------

struct DMGE_API TextureCubeSpecification
{
    uint32_t      Size            = 1;   // faces are square
    TextureFormat Format          = TextureFormat::RGBA8;
    TextureFilter MinFilter       = TextureFilter::Linear;
    TextureFilter MagFilter       = TextureFilter::Linear;
    TextureWrap   WrapR           = TextureWrap::ClampToEdge;
    TextureWrap   WrapS           = TextureWrap::ClampToEdge;
    TextureWrap   WrapT           = TextureWrap::ClampToEdge;
    bool          GenerateMipmaps = true;

    TextureCubeSpecification() = default;
};

// -- TextureCube ---------------------------------------------------

class DMGE_API TextureCube : public Texture
{
public:
    virtual ~TextureCube() = default;

    // Upload pixel data for a single face (face = CubeFace index 0..5).
    virtual void SetData(void* data, uint32_t size, uint32_t face) = 0;

    virtual const TextureCubeSpecification& GetSpecification() const = 0;

    // -- Factory --------------------------------------------------
    static DM::Ref<TextureCube> Create(const TextureCubeSpecification& spec);
    // facePaths order: Right, Left, Top, Bottom, Front, Back.
    static DM::Ref<TextureCube> Create(const std::array<std::string, CubeFaceCount>& facePaths);
};

} // namespace DMGameEngine