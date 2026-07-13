/*
 * DMGameEngine - OpenGL Texture2D
 *
 * OpenGL implementation of the Texture2D abstraction.
 * Creates and manages a GL_TEXTURE_2D object with configurable
 * format, filtering, wrapping and mipmap settings.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture2D.h"

#include <glad/glad.h>

#include <string>
#include <string_view>

namespace DMGameEngine {

class DMGE_API OpenGLTexture2D : public Texture2D
{
public:
    explicit OpenGLTexture2D(const Texture2DSpecification& spec);
    explicit OpenGLTexture2D(std::string_view filepath);
    ~OpenGLTexture2D() override;

    void Bind(uint32_t slot = 0) const override;
    void Unbind()                 const override;

    uint32_t GetWidth()       const override { return m_Spec.Width;  }
    uint32_t GetHeight()      const override { return m_Spec.Height; }
    uint32_t GetRendererID()  const override { return m_RendererID; }

    const Texture2DSpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size) override;

private:
    void Invalidate();   // (re)create the GPU texture from current spec

    uint32_t               m_RendererID = 0;
    Texture2DSpecification m_Spec;
    std::string            m_FilePath;
};

} // namespace DMGameEngine