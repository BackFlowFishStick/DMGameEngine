/*
 * DMGameEngine - OpenGL Vertex Array
 *
 * OpenGL implementation of the VertexArray abstraction.
 * Manages a GL_VERTEX_ARRAY_OBJECT that records vertex attribute
 * bindings (derived from the attached VertexBuffer layouts) and the
 * bound GL_ELEMENT_ARRAY_BUFFER (from the attached IndexBuffer).
 */

#pragma once

#include "DMGameEngine/Renderer/VertexArray.h"

#include <glad/glad.h>

namespace DMGameEngine {

class DMGE_API OpenGLVertexArray : public VertexArray
{
public:
    OpenGLVertexArray();
    ~OpenGLVertexArray() override;

    void Bind()   const override;
    void Unbind() const override;

    void AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer) override;
    void SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer)   override;

    const std::vector<DM::Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
    const DM::Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }

private:
    uint32_t m_RendererID = 0;
    uint32_t m_VertexBufferIndex = 0;
    std::vector<DM::Ref<VertexBuffer>> m_VertexBuffers;
    DM::Ref<IndexBuffer> m_IndexBuffer;
};

} // namespace DMGameEngine