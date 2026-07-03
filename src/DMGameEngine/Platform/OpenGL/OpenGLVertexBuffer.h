/*
 * DMGameEngine - OpenGL Vertex Buffer
 *
 * OpenGL implementation of the VertexBuffer abstraction.
 * Manages a GL_ARRAY_BUFFER for vertex data upload and binding.
 */

#pragma once

#include "DMGameEngine/Renderer/VertexBuffer.h"

#include <glad/glad.h>

namespace DMGameEngine {

class DMGE_API OpenGLVertexBuffer : public VertexBuffer
{
public:
    explicit OpenGLVertexBuffer(uint32_t size);
    OpenGLVertexBuffer(const void* vertices, uint32_t size);
    ~OpenGLVertexBuffer() override;

    void Bind()   const override;
    void Unbind() const override;

    void  SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
    const BufferLayout& GetLayout() const override { return m_Layout; }

    void SetData(const void* data, uint32_t size) override;

private:
    uint32_t     m_RendererID = 0;
    BufferLayout m_Layout;
};

} // namespace DMGameEngine
