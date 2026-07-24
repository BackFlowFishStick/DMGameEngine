/*
 * DMGameEngine - OpenGL Frame Buffer
 *
 * OpenGL implementation of the FrameBuffer abstraction. Owns a GL framebuffer
 * object plus a set of color attachment textures (Texture2D) and an optional
 * depth/stencil attachment texture.
 *
 * Bind() binds the FBO and records the previously-bound target so Unbind()
 * restores it, allowing scene layers to render into their own offscreen
 * targets without clobbering each other. Color/depth attachments are backed
 * by Texture2D (immutable storage via glTexStorage2D), so they can be sampled
 * directly after rendering for render-to-texture use cases.
 *
 * A SwapChainTarget=true spec binds the default framebuffer (0) instead of
 * creating any objects.
 */

#pragma once

#include "DMGameEngine/Renderer/FrameBuffer.h"
#include "DMGameEngine/Renderer/Texture2D.h"

#include <glad/glad.h>

#include <vector>

namespace DMGameEngine {

class DMGE_API OpenGLFrameBuffer : public FrameBuffer
{
public:
    explicit OpenGLFrameBuffer(const FramebufferSpecification& spec);
    ~OpenGLFrameBuffer() override;

    void Bind()   override;
    void Unbind() override;

    void Resize(uint32_t width, uint32_t height) override;

    uint32_t GetWidth()      const override { return m_Spec.Width;  }
    uint32_t GetHeight()     const override { return m_Spec.Height; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    DM::Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const override;
    size_t             GetColorAttachmentCount()        const override { return m_ColorAttachments.size(); }

    const FramebufferSpecification& GetSpecification() const override { return m_Spec; }

private:
    void Invalidate();   // (re)create the FBO + attachments from m_Spec
    void Destroy();

    uint32_t   m_RendererID = 0;
    GLint      m_PrevBoundFBO = 0;   // target saved by Bind(), restored by Unbind()
    FramebufferSpecification m_Spec;

    std::vector<DM::Ref<Texture2D>> m_ColorAttachments;
    DM::Ref<Texture2D>              m_DepthAttachment;
};

} // namespace DMGameEngine