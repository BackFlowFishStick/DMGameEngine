/*
 * DMGameEngine - OpenGL Vertex Buffer Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.h"

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>

namespace DMGameEngine {

// ── Constructors / Destructor ────────────────────────────────────────

OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size)
{
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
}

OpenGLVertexBuffer::OpenGLVertexBuffer(const void* vertices, uint32_t size)
{
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

OpenGLVertexBuffer::~OpenGLVertexBuffer()
{
    glDeleteBuffers(1, &m_RendererID);
}

// ── Bind / Unbind ────────────────────────────────────────────────────

void OpenGLVertexBuffer::Bind() const
{
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
}

void OpenGLVertexBuffer::Unbind() const
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ── Data Upload ──────────────────────────────────────────────────────

void OpenGLVertexBuffer::SetData(const void* data, uint32_t size)
{
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

} // namespace DMGameEngine
