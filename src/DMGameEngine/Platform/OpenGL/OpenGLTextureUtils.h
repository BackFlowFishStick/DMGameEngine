/*
 * DMGameEngine - OpenGL Texture Utilities
 *
 * Shared GL enum-mapping helpers used by every OpenGL texture backend
 * (OpenGLTexture2D, OpenGLTextureCube, OpenGLTexture2DArray). Header-only
 * and inline so each backend picks up the mappings without an extra TU.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture.h"   // TextureFormat / Filter / Wrap
#include "DMGameEngine/Core/Log.h"            // DMGE_CORE_ASSERT

#include <glad/glad.h>

namespace DMGameEngine::Detail {

// internal sized format (e.g. GL_RGBA8)
inline GLenum TextureFormatToGLInternal(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:           return GL_R8;
        case TextureFormat::RG8:          return GL_RG8;
        case TextureFormat::RGB8:         return GL_RGB8;
        case TextureFormat::RGBA8:        return GL_RGBA8;
        case TextureFormat::R16F:         return GL_R16F;
        case TextureFormat::RG16F:        return GL_RG16F;
        case TextureFormat::RGB16F:       return GL_RGB16F;
        case TextureFormat::RGBA16F:      return GL_RGBA16F;
        case TextureFormat::R32F:         return GL_R32F;
        case TextureFormat::RG32F:        return GL_RG32F;
        case TextureFormat::RGB32F:       return GL_RGB32F;
        case TextureFormat::RGBA32F:      return GL_RGBA32F;
        case TextureFormat::Depth:        return GL_DEPTH_COMPONENT24;
        case TextureFormat::DepthStencil: return GL_DEPTH24_STENCIL8;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFormat!");
            return GL_RGBA8;
    }
}

// CPU-side data layout (e.g. GL_RGBA)
inline GLenum TextureFormatToGLData(TextureFormat format)
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

// component data type (e.g. GL_UNSIGNED_BYTE, GL_FLOAT)
inline GLenum TextureFormatToGLType(TextureFormat format)
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

inline GLint TextureFilterToGL(TextureFilter filter)
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

inline GLint TextureWrapToGL(TextureWrap wrap)
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

inline GLsizei FormatChannels(TextureFormat format)
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

} // namespace DMGameEngine::Detail