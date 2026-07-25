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
    void BeginRenderPass(FrameBuffer* target) override;
    void EndRenderPass() override;

private:
    FrameBuffer* m_ActiveTarget = nullptr;
};

} // namespace DMGameEngine