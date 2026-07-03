/*
 * DMGameEngine - Renderer Implementation
 */

#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

Renderer::API Renderer::s_API = Renderer::API::OpenGL;

void Renderer::Init()
{
    DMGE_LOG_INFO("Renderer initialized with API: OpenGL");
}

void Renderer::Shutdown()
{
}

void Renderer::BeginScene()
{
}

void Renderer::EndScene()
{
}

void Renderer::Submit(const VertexArray& /*vertexArray*/)
{
}

void Renderer::Flush()
{
}

void Renderer::OnWindowResize(int /*width*/, int /*height*/)
{
}

} // namespace DMGameEngine

