/*
 * DMGameEngine - Renderer Implementation
 */

#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

Renderer::API Renderer::s_API = Renderer::API::OpenGL;
DM::Scope<RendererAPI> Renderer::s_RendererAPI;
Renderer::SceneData Renderer::s_SceneData;

void Renderer::Init(const RendererAPIInitConfig& config)
{
    s_RendererAPI = RendererAPI::Create();
    DMGE_CORE_ASSERT(s_RendererAPI, "Failed to create RendererAPI backend!");
    s_RendererAPI->Init(config);
    const char* apiName = "Unknown";
    switch (s_API)
    {
        case API::OpenGL:  apiName = "OpenGL";  break;
        case API::Vulkan:   apiName = "Vulkan";   break;
        case API::DirectX:  apiName = "DirectX";  break;
        case API::None:     apiName = "None";     break;
    }
    DMGE_LOG_INFO("Renderer initialized with API: {0}", apiName);
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

void Renderer::SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->SetBlendState(enable, srcFactor, dstFactor);
}

void Renderer::SetBlendEquation(BlendEquation equation)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->SetBlendEquation(equation);
}

void Renderer::SetDepthTest(bool enable)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->SetDepthTest(enable);
}

void Renderer::SetDepthFunc(DepthFunc func)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->SetDepthFunc(func);
}

void Renderer::SetCullMode(CullMode mode)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    s_RendererAPI->SetCullMode(mode);
}

void Renderer::Submit(const DM::Ref<Shader>& shader,
                      const DM::Ref<VertexArray>& vertexArray,
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

void Renderer::Submit(const DM::Ref<Material>& material,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "Renderer not initialized! Call Renderer::Init() first.");
    DMGE_CORE_ASSERT(material, "Renderer::Submit - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::Submit - vertexArray is null!");

    // Bind the shader and upload the stored material uniforms, then
    // supply the scene/object uniforms (u_ViewProjection, u_Transform).
    material->Bind();

    const DM::Ref<Shader>& shader = material->GetShader();
    DMGE_CORE_ASSERT(shader, "Renderer::Submit - material has no shader!");
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
