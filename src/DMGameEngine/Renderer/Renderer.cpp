/*
 * DMGameEngine - Renderer Implementation
 */

#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

Renderer::API Renderer::s_API = Renderer::API::OpenGL;
std::unique_ptr<RendererAPI> Renderer::s_RendererAPI;

void Renderer::Init()
{
    s_RendererAPI = RendererAPI::Create();
    DMGE_CORE_ASSERT(s_RendererAPI, "Failed to create RendererAPI backend!");
    DMGE_LOG_INFO("Renderer initialized with API: OpenGL");
}

void Renderer::Shutdown()
{
    s_RendererAPI.reset();
}

void Renderer::BeginScene()
{
}

void Renderer::EndScene()
{
}

void Renderer::Submit(const VertexArray& vertexArray)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->DrawIndexed(vertexArray);
}

void Renderer::Flush()
{
}

void Renderer::OnWindowResize(int /*width*/, int /*height*/)
{
}

} // namespace DMGameEngine