/*
 * DMGameEngine - OpenGL TextureCube Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLTextureCube.h"

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

// -- Constructors / Destructor -------------------------------------

OpenGLTextureCube::OpenGLTextureCube(const TextureCubeSpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

OpenGLTextureCube::OpenGLTextureCube(const std::array<std::string, CubeFaceCount>& facePaths)
    : m_FacePaths(facePaths)
{
    // Cube-map faces are sampled as-is; do not flip vertically.
    stbi_set_flip_vertically_on_load(0);

    m_Spec.Format = TextureFormat::RGBA8;

    for (uint32_t i = 0; i < CubeFaceCount; ++i)
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc* data = stbi_load(m_FacePaths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!data)
        {
            DMGE_CORE_ASSERT(false, "Failed to load cube face {0} from file: {1}", i, m_FacePaths[i]);
            continue;
        }

        if (i == 0)
        {
            m_Spec.Size = static_cast<uint32_t>(width);
            Invalidate();   // allocate storage once the size is known
        }

        SetData(data, static_cast<uint32_t>(width * height * 4), i);
        stbi_image_free(data);
    }
}

OpenGLTextureCube::~OpenGLTextureCube()
{
    DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));
}

// -- Bind / Unbind -------------------------------------------------

void OpenGLTextureCube::Bind(uint32_t slot) const
{
    DMGE_GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID));
}

void OpenGLTextureCube::Unbind() const
{
    DMGE_GL_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
}

// -- Data Upload ---------------------------------------------------

void OpenGLTextureCube::SetData(void* data, uint32_t size, uint32_t face)
{
    (void)size; // size validated by the caller; we trust the data matches the spec

    DMGE_CORE_ASSERT(face < CubeFaceCount, "Cube face index out of range!");

    Bind(0);

    GLenum dataFormat = TextureFormatToGLData(m_Spec.Format);
    GLenum dataType   = TextureFormatToGLType(m_Spec.Format);

    DMGE_GL_CALL(glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                 0, 0, 0,
                 static_cast<GLsizei>(m_Spec.Size),
                 static_cast<GLsizei>(m_Spec.Size),
                 dataFormat, dataType, data));
}

// -- GPU Resource Creation -----------------------------------------

void OpenGLTextureCube::Invalidate()
{
    if (m_RendererID)
        DMGE_GL_CALL(glDeleteTextures(1, &m_RendererID));

    DMGE_GL_CALL(glGenTextures(1, &m_RendererID));
    Bind(0);

    // -- Filtering ----------------------------------------------
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                    TextureFilterToGL(m_Spec.MinFilter)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                    TextureFilterToGL(m_Spec.MagFilter)));

    // -- Wrapping (seam-free edges for cube maps) ---------------
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
                    TextureWrapToGL(m_Spec.WrapR)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
                    TextureWrapToGL(m_Spec.WrapS)));
    DMGE_GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
                    TextureWrapToGL(m_Spec.WrapT)));

    // -- Allocate each face (no data yet) -----------------------
    GLenum internalFormat = TextureFormatToGLInternal(m_Spec.Format);
    GLenum dataFormat     = TextureFormatToGLData(m_Spec.Format);
    GLenum dataType       = TextureFormatToGLType(m_Spec.Format);

    for (uint32_t i = 0; i < CubeFaceCount; ++i)
    {
        DMGE_GL_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                     0, static_cast<GLint>(internalFormat),
                     static_cast<GLsizei>(m_Spec.Size),
                     static_cast<GLsizei>(m_Spec.Size),
                     0, dataFormat, dataType, nullptr));
    }

    if (m_Spec.GenerateMipmaps)
        DMGE_GL_CALL(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));
}

} // namespace DMGameEngine