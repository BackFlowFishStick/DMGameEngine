/*
 * DMGameEngine - Texture2DArray
 *
 * Array-of-2D-textures abstraction. A single GPU object holding
 * multiple same-sized 2D layers, addressed in-shader via a
 * (u, v, layer) coordinate triple. Derives from Texture and adds
 * array-specific specification, per-layer data upload and a factory.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Texture.h"

#include <cstdint>

namespace DMGameEngine {

// -- Texture2DArray Specification ----------------------------------

struct DMGE_API Texture2DArraySpecification
{
    uint32_t      Width           = 1;
    uint32_t      Height          = 1;
    uint32_t      Layers          = 1;
    TextureFormat Format          = TextureFormat::RGBA8;
    TextureFilter MinFilter       = TextureFilter::Linear;
    TextureFilter MagFilter       = TextureFilter::Linear;
    TextureWrap   WrapS           = TextureWrap::Repeat;
    TextureWrap   WrapT           = TextureWrap::Repeat;
    TextureWrap   WrapR           = TextureWrap::Repeat;
    bool          GenerateMipmaps = true;

    Texture2DArraySpecification() = default;
};

// -- Texture2DArray ------------------------------------------------

class DMGE_API Texture2DArray : public Texture
{
public:
    virtual ~Texture2DArray() = default;

    // Upload pixel data for a single layer (layer index 0..Layers-1).
    virtual void SetData(void* data, uint32_t size, uint32_t layer) = 0;

    virtual uint32_t GetLayerCount() const = 0;

    virtual const Texture2DArraySpecification& GetSpecification() const = 0;

    // -- Factory --------------------------------------------------
    static DM::Ref<Texture2DArray> Create(const Texture2DArraySpecification& spec);
};

} // namespace DMGameEngine