/*
 * DMGameEngine - Render Command Facade
 *
 * Static facade over the active RendererAPI backend. Owns the backend
 * instance and forwards state / draw commands, so that scene-agnostic
 * code (ImGui layers, debug renderers, tools) can issue GPU commands
 * without depending on the scene-level Renderer.
 *
 * The scene-level Renderer delegates its low-level calls here too,
 * keeping its own surface focused on scene submission (BeginScene /
 * EndScene / Submit) and per-frame scene state.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/RendererAPI.h"

namespace DMGameEngine {

class FrameBuffer; // forward declaration (render-target pass)

class DMGE_API RenderCommand
{
public:
    // ── Lifecycle ────────────────────────────────────────────
    //  Creates and initializes the active RendererAPI backend selected
    //  by Renderer::GetAPI(). Must be called before any command below.
    static void Init(const RendererAPIInitConfig& config = {});
    static void Shutdown();

    // ── Framebuffer / draw commands ──────────────────────────
    static void SetClearColor(const glm::vec4& color);
    // Last color set via SetClearColor (cached on the facade so callers -
    // e.g. the deferred lighting pass's sky passthrough - can read it back
    // without a virtual getter on the backend interface).
    static const glm::vec4& GetClearColor();
    static void Clear();
    static void SetViewport(int x, int y, int width, int height);

    // Begins/ends a render pass targeting target (nullptr = swapchain).
    // Forwarded to the active backend. See RendererAPI.
    static void BeginRenderPass(FrameBuffer* target);
    // Desc form (2b stage 1): the backend consumes the full description
    // (attachments/load ops/clear values/viewport). See RendererAPI.
    static void BeginRenderPass(const RenderPassDesc& desc);
    // The backend-annotated desc of the pass currently open (or last
    // begun) - e.g. NdcZMin for depth reprojection. See RendererAPI.
    static RenderPassDesc GetActiveRenderPassDesc();
    static void EndRenderPass();
    static void DrawIndexed(const VertexArray& vertexArray);
    static void DrawIndexedInstanced(const VertexArray& vertexArray, uint32_t instanceCount, uint32_t baseInstance = 0);

    // ── Pipeline state ───────────────────────────────────────
    //  Blend / depth / cull toggles forwarded to the active backend.
    static void SetBlendState(bool enable,
                              BlendFactor srcFactor = BlendFactor::SrcAlpha,
                              BlendFactor dstFactor = BlendFactor::OneMinusSrcAlpha);
    static void SetBlendEquation(BlendEquation equation);

    static void SetDepthTest(bool enable);
    static void SetDepthFunc(DepthFunc func);

    static void SetCullMode(CullMode mode);

private:
    static DM::Scope<RendererAPI> s_RendererAPI;
};

} // namespace DMGameEngine