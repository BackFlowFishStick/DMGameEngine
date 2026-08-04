/*
 * DMGameEngine - OpenGL Frame Buffer Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLFrameBuffer.h"

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"

#include <glad/glad.h>

namespace DMGameEngine {

namespace {

// Depth/stencil TextureFormat -> the GL attachment point the depth attachment
// texture is bound to.
GLenum DepthAttachmentPoint(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::Depth:         return GL_DEPTH_ATTACHMENT;
        case TextureFormat::DepthStencil:  return GL_DEPTH_STENCIL_ATTACHMENT;
        default:
            DMGE_CORE_ASSERT(false, "Non-depth format used for depth attachment!");
            return GL_DEPTH_ATTACHMENT;
    }
}

} // anonymous namespace

// -- Constructors / Destructor -------------------------------------

OpenGLFrameBuffer::OpenGLFrameBuffer(const FramebufferSpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
    Destroy();
}

// -- Bind / Unbind -------------------------------------------------

void OpenGLFrameBuffer::Bind()
{
    // Record the currently-bound draw framebuffer so Unbind() restores it.
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_PrevBoundFBO);

    if (m_Spec.SwapChainTarget)
    {
        DMGE_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        return;
    }

    if (!m_RendererID)
        Invalidate();

    DMGE_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID));
}

void OpenGLFrameBuffer::Unbind()
{
    DMGE_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, m_PrevBoundFBO));
}

// -- Resize --------------------------------------------------------

void OpenGLFrameBuffer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    if (width == m_Spec.Width && height == m_Spec.Height)
        return;

    m_Spec.Width  = width;
    m_Spec.Height = height;

    if (m_Spec.SwapChainTarget)
        return;   // default framebuffer is owned by the window / swapchain

    Invalidate();
}

// -- Attachments ---------------------------------------------------

DM::Ref<Texture2D> OpenGLFrameBuffer::GetColorAttachment(uint32_t index) const
{
    DMGE_CORE_ASSERT(index < m_ColorAttachments.size(),
                     "FrameBuffer color attachment index out of range: {0}", index);
    return m_ColorAttachments[index];
}

// -- GPU resource (re)creation --------------------------------------

void OpenGLFrameBuffer::Invalidate()
{
    Destroy();

    if (m_Spec.SwapChainTarget)
        return;   // default framebuffer needs no objects

    DMGE_GL_CALL(glGenFramebuffers(1, &m_RendererID));

    // Save the caller's current binding and restore it afterwards so
    // (re)creating this target doesn't disturb whatever is bound.
    GLint prevFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    DMGE_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID));

    // -- Color attachments --------------------------------------
    std::vector<GLenum> drawBuffers;
    m_ColorAttachments.clear();
    for (size_t i = 0; i < m_Spec.Attachments.size(); ++i)
    {
        Texture2DSpecification texSpec;
        texSpec.Width  = m_Spec.Width;
        texSpec.Height = m_Spec.Height;
        texSpec.Format = m_Spec.Attachments[i].Format;
        texSpec.MinFilter = TextureFilter::Linear;
        texSpec.MagFilter = TextureFilter::Linear;
        texSpec.WrapS     = TextureWrap::ClampToEdge;
        texSpec.WrapT     = TextureWrap::ClampToEdge;
        texSpec.GenerateMipmaps = false;

        auto tex = Texture2D::Create(texSpec);
        DMGE_GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i),
                        GL_TEXTURE_2D, tex->GetRendererID(), 0));
        drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i));
        m_ColorAttachments.push_back(tex);
    }

    if (drawBuffers.empty())
    {
        // Depth-only target (e.g. shadow map): no color buffers selected.
        DMGE_GL_CALL(glDrawBuffer(GL_NONE));
        DMGE_GL_CALL(glReadBuffer(GL_NONE));
    }
    else if (drawBuffers.size() > 1)
    {
        DMGE_GL_CALL(glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data()));
    }
    else
    {
        DMGE_GL_CALL(glDrawBuffer(GL_COLOR_ATTACHMENT0));
    }

    // -- Depth / stencil attachment -----------------------------
    if (m_Spec.DepthFormat != TextureFormat::None)
    {
        Texture2DSpecification depthSpec;
        depthSpec.Width  = m_Spec.Width;
        depthSpec.Height = m_Spec.Height;
        depthSpec.Format = m_Spec.DepthFormat;
        depthSpec.MinFilter = TextureFilter::Linear;
        depthSpec.MagFilter = TextureFilter::Linear;
        depthSpec.WrapS     = TextureWrap::ClampToEdge;
        depthSpec.WrapT     = TextureWrap::ClampToEdge;
        depthSpec.GenerateMipmaps = false;

        m_DepthAttachment = Texture2D::Create(depthSpec);
        DMGE_GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER,
                        DepthAttachmentPoint(m_Spec.DepthFormat),
                        GL_TEXTURE_2D, m_DepthAttachment->GetRendererID(), 0));
    }

    DMGE_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                     "OpenGL FrameBuffer is incomplete!");

    DMGE_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, prevFBO));
}

void OpenGLFrameBuffer::Destroy()
{
    m_ColorAttachments.clear();   // releases Texture2D -> glDeleteTextures
    m_DepthAttachment.reset();
    if (m_RendererID)
    {
        DMGE_GL_CALL(glDeleteFramebuffers(1, &m_RendererID));
        m_RendererID = 0;
    }
}

} // namespace DMGameEngine