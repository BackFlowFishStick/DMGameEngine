/*
 * DMGameEngine - OpenGL Index Buffer
 *
 * OpenGL implementation of the IndexBuffer abstraction.
 * Manages a GL_ELEMENT_ARRAY_BUFFER for index data upload and binding.
 */

#pragma once

#include "DMGameEngine/Renderer/IndexBuffer.h"

#include <glad/glad.h>

namespace DMGameEngine {

class DMGE_API OpenGLIndexBuffer : public IndexBuffer
{
public:
    OpenGLIndexBuffer(const uint32_t* indices, uint32_t count);
    ~OpenGLIndexBuffer() override;

    void Bind()   const override;
    void Unbind() const override;

    uint32_t GetCount() const override { return m_Count; }

private:
    uint32_t m_RendererID = 0;
    uint32_t m_Count      = 0;
};

} // namespace DMGameEngine
