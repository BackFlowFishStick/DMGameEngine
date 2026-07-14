/*
 * DMGameEngine - OpenGL Texture2DArray
 *
 * OpenGL implementation of the Texture2DArray abstraction.
 * Creates and manages a GL_TEXTURE_2D_ARRAY object: a single GPU
 * resource holding multiple same-sized 2D layers.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture2DArray.h"

#include <glad/glad.h>

namespace DMGameEngine {

class DMGE_API OpenGLTexture2DArray : public Texture2DArray
{
public:
    explicit OpenGLTexture2DArray(const Texture2DArraySpecification& spec);
    ~OpenGLTexture2DArray() override;

    void Bind(uint32_t slot = 0) const override;
    void Unbind()                 const override;

    uint32_t GetWidth()       const override { return m_Spec.Width;  }
    uint32_t GetHeight()      const override { return m_Spec.Height; }
    uint32_t GetLayerCount()  const override { return m_Spec.Layers; }
    uint32_t GetRendererID()  const override { return m_RendererID; }

    const Texture2DArraySpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size, uint32_t layer) override;

    void GenerateMipmaps() override;

private:
    void Invalidate();   // (re)create the GPU texture array from current spec

    uint32_t                    m_RendererID = 0;
    Texture2DArraySpecification m_Spec;
};

} // namespace DMGameEngine