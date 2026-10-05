/*
 * DMGameEngine - OpenGL Renderer API
 *
 * OpenGL implementation of the RendererAPI abstraction.
 * Issues backend-specific draw calls via GLAD/OpenGL.
 */

#pragma once

#include "DMGameEngine/Renderer/RendererAPI.h"

#include <glad/glad.h>

namespace DMGameEngine {

class DMGE_API OpenGLRendererAPI : public RendererAPI
{
public:
    void SetClearColor(const glm::vec4& color) override;
    void Clear() override;
    void SetViewport(int x, int y, int width, int height) override;
    void DrawIndexed(const VertexArray& vertexArray) override;
    void DrawIndexedInstanced(const VertexArray& vertexArray, uint32_t instanceCount, uint32_t baseInstance = 0) override;
    void SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor) override;
    void SetBlendEquation(BlendEquation equation) override;
    void SetDepthTest(bool enable) override;
    void SetDepthFunc(DepthFunc func) override;
    void SetCullMode(CullMode mode) override;

    // -- Render pass / target ----------------------------------------
    // See RendererAPI. nullptr (or a swapchain target) is a no-op so the
    // default framebuffer stays bound; an offscreen FrameBuffer is bound and
    // later unbound (restoring the previous binding).
    //
    // 2b stage 1: the desc form is the single consumption path - the
    // legacy FrameBuffer* overload builds a desc (MakeRenderPassDescForTarget)
    // and delegates. The desc drives the per-load-op clear mask and the
    // optional explicit viewport; glDrawBuffers stays FBO state (K-024).
    void BeginRenderPass(FrameBuffer* target) override;
    void BeginRenderPass(const RenderPassDesc& desc) override;
    void EndRenderPass() override;
    RenderPassDesc GetActiveRenderPassDesc() const override { return m_ActivePass; }

private:
    FrameBuffer* m_ActiveTarget = nullptr;
    // Backend-annotated snapshot of the active pass (NdcZMin = -1: OpenGL
    // maps NDC z [-1,1] -> window [0,1]).
    RenderPassDesc m_ActivePass{};
};

} // namespace DMGameEngine