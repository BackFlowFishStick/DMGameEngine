/*
 * DMGameEngine - Render Command Facade Implementation
 *
 * Owns the active RendererAPI backend instance and forwards each
 * command with an initialization guard. The backend is created in
 * Init() via RendererAPI::Create(), which selects OpenGL / Vulkan /
 * ... based on Renderer::GetAPI().
 */

#include "DMGameEngine/Renderer/RenderCommand.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Debug/Profiler.h"

namespace DMGameEngine {

DM::Scope<RendererAPI> RenderCommand::s_RendererAPI;

// ── Lifecycle ────────────────────────────────────────────────────
void RenderCommand::Init(const RendererAPIInitConfig& config)
{
    s_RendererAPI = RendererAPI::Create();
    DMGE_CORE_ASSERT(s_RendererAPI, "Failed to create RendererAPI backend!");
    s_RendererAPI->Init(config);
}

void RenderCommand::Shutdown()
{
    s_RendererAPI.reset();
}

// ── Framebuffer / draw commands ──────────────────────────────────
void RenderCommand::SetClearColor(const glm::vec4& color)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->SetClearColor(color);
}

void RenderCommand::Clear()
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->Clear();
}

void RenderCommand::SetViewport(int x, int y, int width, int height)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->SetViewport(x, y, width, height);
}

void RenderCommand::DrawIndexed(const VertexArray& vertexArray)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");

    // Account this scene draw in the profiler (draw calls + index
    // count). ImGui draws through its own backend are not routed here.
    uint32_t indexCount = 0;
    if (const auto& indexBuffer = vertexArray.GetIndexBuffer())
        indexCount = indexBuffer->GetCount();
    Profiler::Get().AddDrawCall(indexCount);

    s_RendererAPI->DrawIndexed(vertexArray);
}

// ── Pipeline state ───────────────────────────────────────────────
void RenderCommand::SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->SetBlendState(enable, srcFactor, dstFactor);
}

void RenderCommand::SetBlendEquation(BlendEquation equation)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->SetBlendEquation(equation);
}

void RenderCommand::SetDepthTest(bool enable)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->SetDepthTest(enable);
}

void RenderCommand::SetDepthFunc(DepthFunc func)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->SetDepthFunc(func);
}

void RenderCommand::SetCullMode(CullMode mode)
{
    DMGE_CORE_ASSERT(s_RendererAPI, "RenderCommand not initialized! Call RenderCommand::Init() first.");
    s_RendererAPI->SetCullMode(mode);
}

} // namespace DMGameEngine
