/*
 * DMGameEngine - OpenGL Index Buffer Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLIndexBuffer.h"

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"

namespace DMGameEngine {

// ── Constructor / Destructor ─────────────────────────────────────────

OpenGLIndexBuffer::OpenGLIndexBuffer(const uint32_t* indices, uint32_t count)
    : m_Count(count)
{
    DMGE_GL_CALL(glGenBuffers(1, &m_RendererID));
    DMGE_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID));
    DMGE_GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(count) * sizeof(uint32_t),
                 indices, GL_STATIC_DRAW));
}

OpenGLIndexBuffer::~OpenGLIndexBuffer()
{
    DMGE_GL_CALL(glDeleteBuffers(1, &m_RendererID));
}

// ── Bind / Unbind ────────────────────────────────────────────────────

void OpenGLIndexBuffer::Bind() const
{
    DMGE_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID));
}

void OpenGLIndexBuffer::Unbind() const
{
    DMGE_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

} // namespace DMGameEngine
