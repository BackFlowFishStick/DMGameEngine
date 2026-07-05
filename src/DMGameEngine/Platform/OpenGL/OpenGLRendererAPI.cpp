/*
 * DMGameEngine - OpenGL Renderer API Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"
#include "DMGameEngine/Renderer/VertexArray.h" // full VertexArray / IndexBuffer types

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>

namespace DMGameEngine {

void OpenGLRendererAPI::Init()
{
    glEnable(GL_DEPTH_TEST);
}

void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
{
    glClearColor(color.r, color.g, color.b, color.a);
}

void OpenGLRendererAPI::Clear()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRendererAPI::SetViewport(int x, int y, int width, int height)
{
    glViewport(x, y, width, height);
}

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