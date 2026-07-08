/*
 * DMGameEngine - Renderer Implementation
 */

#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

Renderer::API Renderer::s_API = Renderer::API::OpenGL;
std::unique_ptr<RendererAPI> Renderer::s_RendererAPI;
Renderer::SceneData Renderer::s_SceneData;

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
    s_SceneData.ViewProjectionMatrix = glm::mat4(1.0f);
}

void Renderer::BeginScene(const Camera& camera)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->Clear();
    s_SceneData.ViewProjectionMatrix = camera.GetViewProjection();
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

void Renderer::Submit(const std::shared_ptr<Shader>& shader,
                      const std::shared_ptr<VertexArray>& vertexArray,
                      const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    DMGE_CORE_ASSERT(shader, "Renderer::Submit - shader is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::Submit - vertexArray is null!");

    shader->Bind();
    shader->SetMat4("u_ViewProjection", s_SceneData.ViewProjectionMatrix);
    shader->SetMat4("u_Transform", transform);

    s_RendererAPI->DrawIndexed(*vertexArray);
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
