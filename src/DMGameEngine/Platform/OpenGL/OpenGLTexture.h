/*
 * DMGameEngine - OpenGL Texture
 *
 * OpenGL implementation of the Texture abstraction.
 * Creates and manages a GL texture object with configurable
 * format, filtering, wrapping and mipmap settings.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture.h"

#include <glad/glad.h>

#include <string>
#include <string_view>

namespace DMGameEngine {

class DMGE_API OpenGLTexture : public Texture
{
public:
    explicit OpenGLTexture(const TextureSpecification& spec);
    explicit OpenGLTexture(std::string_view filepath);
    ~OpenGLTexture() override;

    void Bind(uint32_t slot = 0)   const override;
    void Unbind()                  const override;

    uint32_t GetWidth()  const override { return m_Spec.Width;  }
    uint32_t GetHeight() const override { return m_Spec.Height; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    const TextureSpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size) override;

private:
    void Invalidate();   // (re)create the GPU texture from current spec

    static GLenum  TextureFormatToGLInternal(TextureFormat format);
    static GLenum  TextureFormatToGLData(TextureFormat format);
    static GLenum  TextureFormatToGLType(TextureFormat format);
    static GLint   TextureFilterToGL(TextureFilter filter);
    static GLint   TextureWrapToGL(TextureWrap wrap);
    static GLsizei FormatChannels(TextureFormat format);

    uint32_t             m_RendererID = 0;
    TextureSpecification m_Spec;
    std::string          m_FilePath;
};

} // namespace DMGameEngine
