/*
 * DMGameEngine - OpenGL Renderer API Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"
#include "DMGameEngine/Renderer/VertexArray.h" // full VertexArray / IndexBuffer types

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"

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