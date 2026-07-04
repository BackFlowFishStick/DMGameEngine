/*
 * DMGameEngine - OpenGL Renderer API Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"
#include "DMGameEngine/Renderer/VertexArray.h" // full VertexArray / IndexBuffer types

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>

namespace DMGameEngine {

void OpenGLRendererAPI::DrawIndexed(const VertexArray& vertexArray)
{
    vertexArray.Bind();

    const auto& indexBuffer = vertexArray.GetIndexBuffer();
    DMGE_CORE_ASSERT(indexBuffer, "VertexArray has no IndexBuffer attached!");

    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(indexBuffer->GetCount()),
                   GL_UNSIGNED_INT,
                   nullptr);
}

} // namespace DMGameEngine