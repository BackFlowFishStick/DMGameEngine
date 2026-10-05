/*
 * DMGameEngine - OpenGL Renderer API Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"
#include "DMGameEngine/Renderer/VertexArray.h" // full VertexArray / IndexBuffer types

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLFrameBuffer.h"

namespace DMGameEngine {

namespace {

GLenum BlendFactorToGL(BlendFactor factor)
{
    switch (factor)
    {
        case BlendFactor::Zero:                  return GL_ZERO;
        case BlendFactor::One:                   return GL_ONE;
        case BlendFactor::SrcColor:              return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:      return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:              return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor:      return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:              return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:      return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:              return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:      return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor:         return GL_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return GL_ONE_MINUS_CONSTANT_COLOR;
        case BlendFactor::ConstantAlpha:         return GL_CONSTANT_ALPHA;
        case BlendFactor::OneMinusConstantAlpha: return GL_ONE_MINUS_CONSTANT_ALPHA;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendFactor!");
    return GL_NONE;
}

GLenum BlendEquationToGL(BlendEquation equation)
{
    switch (equation)
    {
        case BlendEquation::Add:             return GL_FUNC_ADD;
        case BlendEquation::Subtract:        return GL_FUNC_SUBTRACT;
        case BlendEquation::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
        case BlendEquation::Min:             return GL_MIN;
        case BlendEquation::Max:             return GL_MAX;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendEquation!");
    return GL_NONE;
}

GLenum DepthFuncToGL(DepthFunc func)
{
    switch (func)
    {
        case DepthFunc::Never:         return GL_NEVER;
        case DepthFunc::Less:          return GL_LESS;
        case DepthFunc::Equal:         return GL_EQUAL;
        case DepthFunc::LessEqual:     return GL_LEQUAL;
        case DepthFunc::Greater:       return GL_GREATER;
        case DepthFunc::NotEqual:      return GL_NOTEQUAL;
        case DepthFunc::GreaterEqual:  return GL_GEQUAL;
        case DepthFunc::Always:        return GL_ALWAYS;
    }
    DMGE_CORE_ASSERT(false, "Unknown DepthFunc!");
    return GL_NONE;
}

} // anonymous namespace


void OpenGLRendererAPI::BeginRenderPass(FrameBuffer* target)
{
    // 2b stage 1: legacy form preserved - build the desc from the target's
    // specification and take the single desc consumption path below.
    BeginRenderPass(MakeRenderPassDescForTarget(target));
}

void OpenGLRendererAPI::BeginRenderPass(const RenderPassDesc& desc)
{
    // Snapshot the desc with the backend annotation FIRST so the caller's
    // GetActiveRenderPassDesc() readback is valid even for the swapchain
    // form (which otherwise does nothing - the default framebuffer is
    // already bound and ClearFrame cleared it).
    m_ActivePass       = desc;
    m_ActivePass.NdcZMin = -1.0f; // OpenGL: NDC z [-1,1] -> window [0,1]

    const bool offscreen = desc.Target && !desc.Target->GetSpecification().SwapChainTarget;
    if (!offscreen)
    {
        m_ActiveTarget = nullptr;
        return;
    }

    // The desc must describe exactly what the FBO provides: attachment
    // count mismatches would silently clear the wrong draw buffers
    // (glDrawBuffers is FBO state - K-024's GL-side note).
    DMGE_CORE_ASSERT(desc.ColorAttachmentCount == desc.Target->GetSpecification().Attachments.size(),
                     "OpenGL BeginRenderPass: desc color attachment count does not match the FrameBuffer.");

    desc.Target->Bind();       // saves the previously-bound FBO for Unbind()
    m_ActiveTarget = desc.Target;

    // Clear only what the desc's load ops ask for. LoadOp::Clear maps to
    // the glClear mask bit; Load/DontCare leave the freshly-bound FBO
    // untouched (Load assumes the caller will overwrite or discard the
    // contents). Clear color: renderer-wide clear color (default, preserves
    // the pre-desc behavior) or the desc's explicit value.
    GLbitfield clearMask = 0;
    for (uint32_t i = 0; i < desc.ColorAttachmentCount; ++i)
    {
        if (desc.Color[i].Load == AttachmentLoadOp::Clear)
            clearMask |= GL_COLOR_BUFFER_BIT;
    }
    if (desc.HasDepth && desc.Depth.Load == AttachmentLoadOp::Clear)
        clearMask |= GL_DEPTH_BUFFER_BIT;

    if (clearMask != 0)
    {
        if (!desc.UseRendererClearColor)
            DMGE_GL_CALL(glClearColor(desc.ClearColor[0], desc.ClearColor[1],
                                      desc.ClearColor[2], desc.ClearColor[3]));
        if (clearMask & GL_DEPTH_BUFFER_BIT)
            DMGE_GL_CALL(glClearDepth(desc.Depth.ClearDepth));
        DMGE_GL_CALL(glClear(clearMask));
    }

    // Explicit viewport (UseTargetExtent=false only; the default leaves the
    // viewport as the host set it - pre-desc behavior).
    if (!desc.UseTargetExtent)
        DMGE_GL_CALL(glViewport(static_cast<GLint>(desc.ViewportX),
                                static_cast<GLint>(desc.ViewportY),
                                static_cast<GLsizei>(desc.ViewportWidth),
                                static_cast<GLsizei>(desc.ViewportHeight)));
}

void OpenGLRendererAPI::EndRenderPass()
{
    if (m_ActiveTarget)
    {
        m_ActiveTarget->Unbind(); // restores the previously-bound FBO
        m_ActiveTarget = nullptr;
    }
}
void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
{
    DMGE_GL_CALL(glClearColor(color.r, color.g, color.b, color.a));
}

void OpenGLRendererAPI::Clear()
{
    DMGE_GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void OpenGLRendererAPI::SetViewport(int x, int y, int width, int height)
{
    DMGE_GL_CALL(glViewport(x, y, width, height));
}

void OpenGLRendererAPI::DrawIndexed(const VertexArray& vertexArray)
{
    vertexArray.Bind();

    const auto& indexBuffer = vertexArray.GetIndexBuffer();
    DMGE_CORE_ASSERT(indexBuffer, "VertexArray has no IndexBuffer attached!");

    DMGE_GL_CALL(glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(indexBuffer->GetCount()),
                   GL_UNSIGNED_INT,
                   nullptr));
}

void OpenGLRendererAPI::DrawIndexedInstanced(const VertexArray& vertexArray,
                                             uint32_t instanceCount,
                                             uint32_t baseInstance)
{
    vertexArray.Bind();

    const auto& indexBuffer = vertexArray.GetIndexBuffer();
    DMGE_CORE_ASSERT(indexBuffer, "VertexArray has no IndexBuffer attached!");

    DMGE_GL_CALL(glDrawElementsInstanced(GL_TRIANGLES,
                   static_cast<GLsizei>(indexBuffer->GetCount()),
                   GL_UNSIGNED_INT,
                   nullptr,
                   static_cast<GLsizei>(instanceCount)));

    // baseInstance requires GL 4.2 glDrawElementsInstancedBaseInstance;
    // not issued here to stay compatible with core 3.3 loaders.
    (void)baseInstance;
}

void OpenGLRendererAPI::SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor)
{
    if (enable)
    {
        DMGE_GL_CALL(glEnable(GL_BLEND));
        DMGE_GL_CALL(glBlendFunc(BlendFactorToGL(srcFactor), BlendFactorToGL(dstFactor)));
    }
    else
    {
        DMGE_GL_CALL(glDisable(GL_BLEND));
    }
}

void OpenGLRendererAPI::SetBlendEquation(BlendEquation equation)
{
    DMGE_GL_CALL(glBlendEquation(BlendEquationToGL(equation)));
}

void OpenGLRendererAPI::SetDepthTest(bool enable)
{
    if (enable)
        DMGE_GL_CALL(glEnable(GL_DEPTH_TEST));
    else
        DMGE_GL_CALL(glDisable(GL_DEPTH_TEST));
}

void OpenGLRendererAPI::SetDepthFunc(DepthFunc func)
{
    DMGE_GL_CALL(glDepthFunc(DepthFuncToGL(func)));
}

void OpenGLRendererAPI::SetCullMode(CullMode mode)
{
    if (mode == CullMode::None)
    {
        DMGE_GL_CALL(glDisable(GL_CULL_FACE));
        return;
    }

    DMGE_GL_CALL(glEnable(GL_CULL_FACE));
    switch (mode)
    {
        case CullMode::Front:        DMGE_GL_CALL(glCullFace(GL_FRONT));          break;
        case CullMode::Back:         DMGE_GL_CALL(glCullFace(GL_BACK));           break;
        case CullMode::FrontAndBack: DMGE_GL_CALL(glCullFace(GL_FRONT_AND_BACK)); break;
        case CullMode::None:         break;
    }
}

} // namespace DMGameEngine