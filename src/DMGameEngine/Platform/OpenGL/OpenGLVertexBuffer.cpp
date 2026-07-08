/*
 * DMGameEngine - OpenGL Vertex Buffer Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.h"

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"

namespace DMGameEngine {

// ── Constructors / Destructor ────────────────────────────────────────

OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size)
{
    DMGE_GL_CALL(glGenBuffers(1, &m_RendererID));
    DMGE_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_RendererID));
    DMGE_GL_CALL(glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW));
}

OpenGLVertexBuffer::OpenGLVertexBuffer(const void* vertices, uint32_t size)
{
    DMGE_GL_CALL(glGenBuffers(1, &m_RendererID));
    DMGE_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_RendererID));
    DMGE_GL_CALL(glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW));
}

OpenGLVertexBuffer::~OpenGLVertexBuffer()
{
    DMGE_GL_CALL(glDeleteBuffers(1, &m_RendererID));
}

// ── Bind / Unbind ────────────────────────────────────────────────────

void OpenGLVertexBuffer::Bind() const
{
    DMGE_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_RendererID));
}

void OpenGLVertexBuffer::Unbind() const
{
    DMGE_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

// ── Data Upload ──────────────────────────────────────────────────────

void OpenGLVertexBuffer::SetData(const void* data, uint32_t size)
{
    DMGE_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_RendererID));
    DMGE_GL_CALL(glBufferSubData(GL_ARRAY_BUFFER, 0, size, data));
}

} // namespace DMGameEngine
