/*
 * DMGameEngine - OpenGL Index Buffer Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLIndexBuffer.h"

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>

namespace DMGameEngine {

// ── Constructor / Destructor ─────────────────────────────────────────

OpenGLIndexBuffer::OpenGLIndexBuffer(const uint32_t* indices, uint32_t count)
    : m_Count(count)
{
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(count) * sizeof(uint32_t),
                 indices, GL_STATIC_DRAW);
}

OpenGLIndexBuffer::~OpenGLIndexBuffer()
{
    glDeleteBuffers(1, &m_RendererID);
}

// ── Bind / Unbind ────────────────────────────────────────────────────

void OpenGLIndexBuffer::Bind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
}

void OpenGLIndexBuffer::Unbind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace DMGameEngine
