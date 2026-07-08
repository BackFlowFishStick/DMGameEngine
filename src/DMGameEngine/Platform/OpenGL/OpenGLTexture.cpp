/*
 * DMGameEngine - OpenGL Texture Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLTexture.h"

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"
#include <stb_image.h>

namespace DMGameEngine {

// ── Format Mapping Helpers ───────────────────────────────────────

GLenum OpenGLTexture::TextureFormatToGLInternal(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:          return GL_R8;
        case TextureFormat::RG8:         return GL_RG8;
        case TextureFormat::RGB8:        return GL_RGB8;
        case TextureFormat::RGBA8:       return GL_RGBA8;
        case TextureFormat::R16F:        return GL_R16F;
        case TextureFormat::RG16F:       return GL_RG16F;
        case TextureFormat::RGB16F:      return GL_RGB16F;
        case TextureFormat::RGBA16F:     return GL_RGBA16F;
        case TextureFormat::R32F:        return GL_R32F;
        case TextureFormat::RG32F:       return GL_RG32F;
        case TextureFormat::RGB32F:      return GL_RGB32F;
        case TextureFormat::RGBA32F:     return GL_RGBA32F;
        case TextureFormat::Depth:       return GL_DEPTH_COMPONENT24;
        case TextureFormat::DepthStencil:return GL_DEPTH24_STENCIL8;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFormat!");
            return GL_RGBA8;
    }
}

GLenum OpenGLTexture::TextureFormatToGLData(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:
        case TextureFormat::R16F:
        case TextureFormat::R32F:
            return GL_RED;

        case TextureFormat::RG8:
        case TextureFormat::RG16F:
        case TextureFormat::RG32F:
            return GL_RG;

        case TextureFormat::RGB8:
        case TextureFormat::RGB16F:
        case TextureFormat::RGB32F:
            return GL_RGB;

        case TextureFormat::RGBA8:
        case TextureFormat::RGBA16F:
        case TextureFormat::RGBA32F:
            return GL_RGBA;

        case TextureFormat::Depth:
            return GL_DEPTH_COMPONENT;

        case TextureFormat::DepthStencil:
            return GL_DEPTH_STENCIL;

        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFormat!");
            return GL_RGBA;
    }
}

GLenum OpenGLTexture::TextureFormatToGLType(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:
        case TextureFormat::RG8:
        case TextureFormat::RGB8:
        case TextureFormat::RGBA8:
            return GL_UNSIGNED_BYTE;

        case TextureFormat::R16F:
        case TextureFormat::RG16F:
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F:
            return GL_FLOAT;

        case TextureFormat::R32F:
        case TextureFormat::RG32F:
        case TextureFormat::RGB32F:
        case TextureFormat::RGBA32F:
            return GL_FLOAT;

        case TextureFormat::Depth:
        case TextureFormat::DepthStencil:
            return GL_UNSIGNED_BYTE;

        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFormat!");
            return GL_UNSIGNED_BYTE;
    }
}

GLint OpenGLTexture::TextureFilterToGL(TextureFilter filter)
{
    switch (filter)
    {
        case TextureFilter::Nearest: return GL_NEAREST;
        case TextureFilter::Linear:  return GL_LINEAR;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFilter!");
            return GL_LINEAR;
    }
}

GLint OpenGLTexture::TextureWrapToGL(TextureWrap wrap)
{
    switch (wrap)
    {
        case TextureWrap::Repeat:         return GL_REPEAT;
        case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder:  return GL_CLAMP_TO_BORDER;
        case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureWrap!");
            return GL_REPEAT;
    }
}

GLsizei OpenGLTexture::FormatChannels(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:
        case TextureFormat::R16F:
        case TextureFormat::R32F:
        case TextureFormat::Depth:
            return 1;

        case TextureFormat::RG8:
        case TextureFormat::RG16F:
        case TextureFormat::RG32F:
        case TextureFormat::DepthStencil:
            return 2;

        case TextureFormat::RGB8:
        case TextureFormat::RGB16F:
        case TextureFormat::RGB32F:
            return 3;

        case TextureFormat::RGBA8:
        case TextureFormat::RGBA16F:
        case TextureFormat::RGBA32F:
            return 4;

        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFormat!");
            return 4;
    }
}

// ── Constructors / Destructor ────────────────────────────────────

OpenGLTexture::OpenGLTexture(const TextureSpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

OpenGLTexture::OpenGLTexture(std::string_view filepath)
    : m_FilePath(filepath)
{
    // Flip textures on load — OpenGL expects (0,0) at bottom-left
    stbi_set_flip_vertically_on_load(1);

    int width = 0, height = 0, channels = 0;
    stbi_uc* data = stbi_load(m_FilePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!data)
    {
        DMGE_CORE_ASSERT(false, "Failed to load texture from file: {0}", m_FilePath);
        return;
    }

    m_Spec.Width  = static_cast<uint32_t>(width);
    m_Spec.Height = static_cast<uint32_t>(height);
    m_Spec.Format = TextureFormat::RGBA8;

    Invalidate();
    SetData(data, static_cast<uint32_t>(width * height * 4));

    stbi_image_free(data);
}

OpenGLTexture::~OpenGLTexture()
{
    DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));
}

// ── Bind / Unbind ────────────────────────────────────────────────

void OpenGLTexture::Bind(uint32_t slot) const
{
    DMGE_GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_2D, m_RendererID));
}

void OpenGLTexture::Unbind() const
{
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

// ── Data Upload ──────────────────────────────────────────────────

void OpenGLTexture::SetData(void* data, uint32_t size)
{
    (void)size; // size validated by the caller; we trust the data matches the spec

    Bind(0);

    GLenum internalFormat = TextureFormatToGLInternal(m_Spec.Format);
    GLenum dataFormat     = TextureFormatToGLData(m_Spec.Format);
    GLenum dataType       = TextureFormatToGLType(m_Spec.Format);

    DMGE_GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat),
                 static_cast<GLsizei>(m_Spec.Width),
                 static_cast<GLsizei>(m_Spec.Height),
                 0, dataFormat, dataType, data));

    if (m_Spec.GenerateMipmaps)
        DMGE_GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
}

// ── GPU Resource Creation ────────────────────────────────────────

void OpenGLTexture::Invalidate()
{
    if (m_RendererID)
        DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));

    DMGE_GL_CALL(glGenTextures(1, &m_RendererID));
    Bind(0);

    // ── Filtering ───────────────────────────────────────────────
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    TextureFilterToGL(m_Spec.MinFilter)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    TextureFilterToGL(m_Spec.MagFilter)));

    // ── Wrapping ────────────────────────────────────────────────
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    TextureWrapToGL(m_Spec.WrapS)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    TextureWrapToGL(m_Spec.WrapT)));

    // ── Allocate immutable storage (no data yet) ────────────────
    GLenum internalFormat = TextureFormatToGLInternal(m_Spec.Format);
    GLenum dataFormat     = TextureFormatToGLData(m_Spec.Format);
    GLenum dataType       = TextureFormatToGLType(m_Spec.Format);

    DMGE_GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat),
                 static_cast<GLsizei>(m_Spec.Width),
                 static_cast<GLsizei>(m_Spec.Height),
                 0, dataFormat, dataType, nullptr));

    if (m_Spec.GenerateMipmaps)
        DMGE_GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
}

} // namespace DMGameEngine
