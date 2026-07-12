/*
 * DMGameEngine - Texture Abstraction
 *
 * Base class for all graphics API texture implementations.
 * Platform backends (OpenGL, Vulkan, DirectX) derive from this
 * and provide their own texture creation, binding and data upload.
 *
 * Textures are created via the static Create() factory, which
 * selects the correct backend based on the active Renderer::API.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"
#include <string>
#include <string_view>
#include <memory>
#include <cstdint>
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

// ── Texture Format ─────────────────────────────────────────────────

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

// ── Texture Filter ─────────────────────────────────────────────────

enum class TextureFilter : uint8_t
{
    None     = 0,
    Nearest  = 1,
    Linear   = 2
};

// ── Texture Wrap ───────────────────────────────────────────────────

enum class TextureWrap : uint8_t
{
    None         = 0,
    Repeat       = 1,
    ClampToEdge  = 2,
    ClampToBorder = 3,
    MirroredRepeat = 4
};

// ── Texture Specification ──────────────────────────────────────────

struct DMGE_API TextureSpecification
{
    uint32_t      Width          = 1;
    uint32_t      Height         = 1;
    TextureFormat Format         = TextureFormat::RGBA8;
    TextureFilter MinFilter      = TextureFilter::Linear;
    TextureFilter MagFilter      = TextureFilter::Linear;
    TextureWrap   WrapS          = TextureWrap::Repeat;
    TextureWrap   WrapT          = TextureWrap::Repeat;
    bool          GenerateMipmaps = true;

    TextureSpecification() = default;
};

// ── Texture ────────────────────────────────────────────────────────

class DMGE_API Texture
{
public:
    virtual ~Texture() = default;

    virtual void Bind(uint32_t slot = 0)   const = 0;
    virtual void Unbind()                  const = 0;

    virtual uint32_t GetWidth()  const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual uint32_t GetRendererID() const = 0;

    virtual const TextureSpecification& GetSpecification() const = 0;

    virtual void SetData(void* data, uint32_t size) = 0;

    virtual bool operator==(const Texture& other) const
    {
        return GetRendererID() == other.GetRendererID();
    }

    // ── Factory ─────────────────────────────────────────────────
    static DM::Ref<Texture> Create(const TextureSpecification& spec);
    static DM::Ref<Texture> Create(std::string_view filepath);
};

} // namespace DMGameEngine
