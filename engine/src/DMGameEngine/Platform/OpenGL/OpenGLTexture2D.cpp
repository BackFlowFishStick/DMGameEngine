/*
 * DMGameEngine - OpenGL Texture2D Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLTexture2D.h"

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLTextureUtils.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace DMGameEngine {

using Detail::TextureFormatToGLInternal;
using Detail::TextureFormatToGLData;
using Detail::TextureFormatToGLType;
using Detail::TextureFilterToGL;
using Detail::TextureWrapToGL;
using Detail::MipLevelCount;

// -- Constructors / Destructor -------------------------------------

OpenGLTexture2D::OpenGLTexture2D(const Texture2DSpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

OpenGLTexture2D::OpenGLTexture2D(std::string_view filepath)
    : m_FilePath(filepath)
{
    // Flip textures on load - OpenGL expects (0,0) at bottom-left
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

    if (m_Spec.GenerateMipmaps)
        GenerateMipmaps();

    stbi_image_free(data);
}

OpenGLTexture2D::~OpenGLTexture2D()
{
    DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));
}

// -- Bind / Unbind -------------------------------------------------

void OpenGLTexture2D::Bind(uint32_t slot) const
{
    DMGE_GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_2D, m_RendererID));
}

void OpenGLTexture2D::Unbind() const
{
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

// -- Data Upload ---------------------------------------------------

void OpenGLTexture2D::SetData(void* data, uint32_t size)
{
    (void)size; // size validated by the caller; we trust the data matches the spec

    Bind(0);

    GLenum dataFormat = TextureFormatToGLData(m_Spec.Format);
    GLenum dataType   = TextureFormatToGLType(m_Spec.Format);

    // Update the whole base level of the immutable storage allocated in
    // Invalidate(). glTexSubImage2D only writes pixels into existing storage
    // (no re-spec / reallocation), making it suitable for frequent updates
    // (video frames, dynamic canvases, ...). Mipmaps are NOT regenerated
    // here - call GenerateMipmaps() explicitly when needed.
    DMGE_GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                 static_cast<GLsizei>(m_Spec.Width),
                 static_cast<GLsizei>(m_Spec.Height),
                 dataFormat, dataType, data));
}

// -- Mipmaps -------------------------------------------------------

void OpenGLTexture2D::GenerateMipmaps()
{
    if (!m_RendererID || !m_Spec.GenerateMipmaps)
        return;   // no texture / storage was allocated with a single level

    Bind(0);
    DMGE_GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
}

// -- GPU Resource Creation -----------------------------------------

void OpenGLTexture2D::Invalidate()
{
    if (m_RendererID)
        DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));

    DMGE_GL_CALL(glGenTextures(1, &m_RendererID));
    Bind(0);

    // -- Filtering ----------------------------------------------
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    TextureFilterToGL(m_Spec.MinFilter)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    TextureFilterToGL(m_Spec.MagFilter)));

    // -- Wrapping -----------------------------------------------
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    TextureWrapToGL(m_Spec.WrapS)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    TextureWrapToGL(m_Spec.WrapT)));

    // -- Allocate immutable storage (no data yet) ---------------
    // Storage is allocated once; mip levels are reserved up-front when
    // GenerateMipmaps is set so GenerateMipmaps() can fill them later.
    // Subsequent updates must use glTexSubImage2D (glTexImage2D is illegal
    // on immutable-storage textures).
    GLenum internalFormat = TextureFormatToGLInternal(m_Spec.Format);
    GLsizei levels = m_Spec.GenerateMipmaps
        ? MipLevelCount(m_Spec.Width, m_Spec.Height)
        : 1;

    DMGE_GL_CALL(glTexStorage2D(GL_TEXTURE_2D, levels, internalFormat,
                 static_cast<GLsizei>(m_Spec.Width),
                 static_cast<GLsizei>(m_Spec.Height)));
}

} // namespace DMGameEngine