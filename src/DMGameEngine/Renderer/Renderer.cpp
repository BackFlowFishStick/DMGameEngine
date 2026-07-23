/*
 * DMGameEngine - Renderer Implementation
 *
 * Scene-level surface: brackets a frame with BeginScene / EndScene,
 * caches the camera view-projection, and enqueues draws into a
 * RenderQueue. The queue is sorted by material/shader and flushed at
 * EndScene / Flush, binding each group's state only once per frame
 * instead of once per Submit.
 */

#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/RenderCommand.h"
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

Renderer::API Renderer::s_API = Renderer::API::OpenGL;
Renderer::SceneData Renderer::s_SceneData;
RenderQueue Renderer::s_Queue;

void Renderer::Init(const RendererAPIInitConfig& config)
{
    RenderCommand::Init(config);

    const char* apiName = "Unknown";
    switch (s_API)
    {
        case API::OpenGL:  apiName = "OpenGL";  break;
        case API::Vulkan:  apiName = "Vulkan";   break;
        case API::DirectX: apiName = "DirectX";  break;
        case API::None:    apiName = "None";     break;
    }
    DMGE_LOG_INFO("Renderer initialized with API: {0}", apiName);
}

void Renderer::Shutdown()
{
    RenderCommand::Shutdown();
}

void Renderer::BeginScene()
{
    RenderCommand::Clear();
    s_SceneData.ViewProjectionMatrix = glm::mat4(1.0f);
    s_Queue.Clear();
}

void Renderer::BeginScene(const Camera& camera)
{
    RenderCommand::Clear();
    s_SceneData.ViewProjectionMatrix = camera.GetViewProjection();
    if (s_API == API::Vulkan)
    {
        // Vulkan clip space Y points down (OpenGL Y points up), so an
        // OpenGL-convention projection renders upside down. Negate the
        // clip-space Y here; the matching frontFace flip in the Vulkan
        // pipeline keeps back-face culling behaving as under OpenGL.
        glm::mat4 flipY(1.0f);
        flipY[1][1] = -1.0f;
        s_SceneData.ViewProjectionMatrix = flipY * s_SceneData.ViewProjectionMatrix;
    }
    s_Queue.Clear();
}

void Renderer::EndScene()
{
    Flush();
}

void Renderer::Submit(const DM::Ref<Shader>& shader,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(shader, "Renderer::Submit - shader is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::Submit - vertexArray is null!");
    s_Queue.Submit(shader, vertexArray, transform);
}

void Renderer::Submit(const DM::Ref<Material>& material,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(material, "Renderer::Submit - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::Submit - vertexArray is null!");
    s_Queue.Submit(material, vertexArray, transform);
}

void Renderer::Flush()
{
    s_Queue.Flush(s_SceneData.ViewProjectionMatrix);
}

void Renderer::OnWindowResize(int width, int height)
{
    RenderCommand::SetViewport(0, 0, width, height);
}

} // namespace DMGameEngine