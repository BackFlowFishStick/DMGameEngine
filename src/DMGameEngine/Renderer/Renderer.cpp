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
    s_RendererAPI->Init();
    DMGE_LOG_INFO("Renderer initialized with API: OpenGL");
}

void Renderer::Shutdown()
{
    s_RendererAPI.reset();
}

void Renderer::BeginScene()
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->Clear();
}

void Renderer::EndScene()
{
}

void Renderer::SetClearColor(const glm::vec4& color)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->SetClearColor(color);
}

void Renderer::Clear()
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->Clear();
}

void Renderer::Submit(const VertexArray& vertexArray)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->DrawIndexed(vertexArray);
}

void Renderer::Flush()
{
}

void Renderer::OnWindowResize(int width, int height)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->SetViewport(0, 0, width, height);
}

} // namespace DMGameEngine