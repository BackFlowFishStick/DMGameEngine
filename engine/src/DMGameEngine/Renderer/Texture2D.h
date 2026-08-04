/*
 * DMGameEngine - Texture2D
 *
 * 2D texture abstraction. Derives from Texture and adds the
 * 2D-specific specification, per-texel data upload and a factory
 * that selects the correct backend based on the active Renderer::API.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Texture.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace DMGameEngine {

// -- Texture2D Specification ---------------------------------------

struct DMGE_API Texture2DSpecification
{
    uint32_t      Width           = 1;
    uint32_t      Height          = 1;
    TextureFormat Format          = TextureFormat::RGBA8;
    TextureFilter MinFilter       = TextureFilter::Linear;
    TextureFilter MagFilter       = TextureFilter::Linear;
    TextureWrap   WrapS           = TextureWrap::Repeat;
    TextureWrap   WrapT           = TextureWrap::Repeat;
    bool          GenerateMipmaps = true;

    Texture2DSpecification() = default;
};

// -- Texture2D -----------------------------------------------------

class DMGE_API Texture2D : public Texture
{
public:
    virtual ~Texture2D() = default;

    virtual void SetData(void* data, uint32_t size) = 0;

    virtual const Texture2DSpecification& GetSpecification() const = 0;

    // -- Factory --------------------------------------------------
    static DM::Ref<Texture2D> Create(const Texture2DSpecification& spec);
    static DM::Ref<Texture2D> Create(std::string_view filepath);
};

} // namespace DMGameEngine