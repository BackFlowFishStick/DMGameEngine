/*
 * DMGameEngine - Texture Abstraction
 *
 * Abstract base class for all graphics API texture implementations.
 * Platform backends (OpenGL, Vulkan, DirectX) derive from this and
 * provide their own texture creation, binding and data upload.
 *
 * Concrete texture kinds (Texture2D, TextureCube, Texture2DArray)
 * derive from Texture and expose type-specific specification structs
 * and Create() factories.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

#include <cstdint>

namespace DMGameEngine {

// -- Texture Format ------------------------------------------------

enum class TextureFormat : uint8_t
{
    None    = 0,
    R8,
    RG8,
    RGB8,
    RGBA8,
    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    Depth,
    DepthStencil
};

// -- Texture Filter ------------------------------------------------

enum class TextureFilter : uint8_t
{
    None     = 0,
    Nearest  = 1,
    Linear   = 2
};

// -- Texture Wrap --------------------------------------------------

enum class TextureWrap : uint8_t
{
    None           = 0,
    Repeat         = 1,
    ClampToEdge    = 2,
    ClampToBorder  = 3,
    MirroredRepeat = 4
};

// -- Texture (base) -----------------------------------------------

class DMGE_API Texture
{
public:
    virtual ~Texture() = default;

    virtual uint32_t GetWidth()      const = 0;
    virtual uint32_t GetHeight()     const = 0;
    virtual uint32_t GetRendererID() const = 0;

    virtual void Bind(uint32_t slot = 0) const = 0;
    virtual void Unbind()                 const = 0;

    virtual bool operator==(const Texture& other) const
    {
        return GetRendererID() == other.GetRendererID();
    }

    // Regenerate the mipmap chain from the base level. No-op for textures
    // whose storage was allocated without mip levels (GenerateMipmaps=false).
    virtual void GenerateMipmaps() = 0;
};

} // namespace DMGameEngine