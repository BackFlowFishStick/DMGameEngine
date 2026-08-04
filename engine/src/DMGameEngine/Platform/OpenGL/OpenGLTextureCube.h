/*
 * DMGameEngine - OpenGL TextureCube
 *
 * OpenGL implementation of the TextureCube abstraction.
 * Creates and manages a GL_TEXTURE_CUBE_MAP object; each of the six
 * faces shares one format / size / filtering configuration.
 */

#pragma once

#include "DMGameEngine/Renderer/TextureCube.h"

#include <glad/glad.h>

#include <array>
#include <string>

namespace DMGameEngine {

class DMGE_API OpenGLTextureCube : public TextureCube
{
public:
    explicit OpenGLTextureCube(const TextureCubeSpecification& spec);
    explicit OpenGLTextureCube(const std::array<std::string, CubeFaceCount>& facePaths);
    ~OpenGLTextureCube() override;

    void Bind(uint32_t slot = 0) const override;
    void Unbind()                 const override;

    uint32_t GetWidth()      const override { return m_Spec.Size; }
    uint32_t GetHeight()     const override { return m_Spec.Size; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    const TextureCubeSpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size, uint32_t face) override;

    void GenerateMipmaps() override;

private:
    void Invalidate();   // (re)create the GPU cube map from current spec

    uint32_t                 m_RendererID = 0;
    TextureCubeSpecification m_Spec;
    std::array<std::string, CubeFaceCount> m_FacePaths;
};

} // namespace DMGameEngine