/*
 * DMGameEngine - OpenGL Texture2DArray Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLTexture2DArray.h"

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLTextureUtils.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"

#include <glad/glad.h>

namespace DMGameEngine {

using Detail::TextureFormatToGLInternal;
using Detail::TextureFormatToGLData;
using Detail::TextureFormatToGLType;
using Detail::TextureFilterToGL;
using Detail::TextureWrapToGL;
using Detail::MipLevelCount;

// -- Constructor / Destructor --------------------------------------

OpenGLTexture2DArray::OpenGLTexture2DArray(const Texture2DArraySpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

OpenGLTexture2DArray::~OpenGLTexture2DArray()
{
    DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));
}

// -- Bind / Unbind -------------------------------------------------

void OpenGLTexture2DArray::Bind(uint32_t slot) const
{
    DMGE_GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_2D_ARRAY, m_RendererID));
}

void OpenGLTexture2DArray::Unbind() const
{
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
}

// -- Data Upload ---------------------------------------------------

void OpenGLTexture2DArray::SetData(void* data, uint32_t size, uint32_t layer)
{
    (void)size; // size validated by the caller; we trust the data matches the spec

    DMGE_CORE_ASSERT(layer < m_Spec.Layers, "Texture2DArray layer index out of range!");

    Bind(0);

    GLenum dataFormat = TextureFormatToGLData(m_Spec.Format);
    GLenum dataType   = TextureFormatToGLType(m_Spec.Format);

    // Update a single layer (depth = 1) within the allocated array.
    DMGE_GL_CALL(glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                 0,
                 0, 0, static_cast<GLint>(layer),
                 static_cast<GLsizei>(m_Spec.Width),
                 static_cast<GLsizei>(m_Spec.Height),
                 1,
                 dataFormat, dataType, data));
}

// -- Mipmaps -------------------------------------------------------

void OpenGLTexture2DArray::GenerateMipmaps()
{
    if (!m_RendererID || !m_Spec.GenerateMipmaps)
        return;   // no texture / storage was allocated with a single level

    Bind(0);
    DMGE_GL_CALL(glGenerateMipmap(GL_TEXTURE_2D_ARRAY));
}

// -- GPU Resource Creation -----------------------------------------

void OpenGLTexture2DArray::Invalidate()
{
    if (m_RendererID)
        DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));

    DMGE_GL_CALL(glGenTextures(1, &m_RendererID));
    Bind(0);

    // -- Filtering ----------------------------------------------
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                    TextureFilterToGL(m_Spec.MinFilter)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,
                    TextureFilterToGL(m_Spec.MagFilter)));

    // -- Wrapping -----------------------------------------------
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                    TextureWrapToGL(m_Spec.WrapS)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                    TextureWrapToGL(m_Spec.WrapT)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                    TextureWrapToGL(m_Spec.WrapR)));

    // -- Allocate immutable array storage (no data yet) ---------
    // Storage is allocated once; mip levels are reserved up-front when
    // GenerateMipmaps is set so GenerateMipmaps() can fill them later.
    // Per-layer updates must use glTexSubImage3D (glTexImage3D is illegal
    // on immutable-storage textures).
    GLenum internalFormat = TextureFormatToGLInternal(m_Spec.Format);
    GLsizei levels = m_Spec.GenerateMipmaps
        ? MipLevelCount(m_Spec.Width, m_Spec.Height)
        : 1;

    DMGE_GL_CALL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internalFormat,
                 static_cast<GLsizei>(m_Spec.Width),
                 static_cast<GLsizei>(m_Spec.Height),
                 static_cast<GLsizei>(m_Spec.Layers)));
}

} // namespace DMGameEngine