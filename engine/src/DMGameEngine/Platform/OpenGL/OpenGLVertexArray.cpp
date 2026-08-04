/*
 * DMGameEngine - OpenGL Vertex Array Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLVertexArray.h"
#include "DMGameEngine/Renderer/Shader.h" // ShaderDataType, BufferElement, BufferLayout

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>
#include "DMGameEngine/Platform/OpenGL/OpenGLDebug.h"

namespace DMGameEngine {

namespace {

GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
{
    switch (type)
    {
        case ShaderDataType::Float:  case ShaderDataType::Float2:
        case ShaderDataType::Float3: case ShaderDataType::Float4:
        case ShaderDataType::Mat3:   case ShaderDataType::Mat4:
            return GL_FLOAT;

        case ShaderDataType::Int:  case ShaderDataType::Int2:
        case ShaderDataType::Int3: case ShaderDataType::Int4:
            return GL_INT;

        case ShaderDataType::Bool:
            return GL_BOOL;

        default:
            DMGE_CORE_ASSERT(false, "Unknown ShaderDataType!");
            return GL_NONE;
    }
}

} // namespace

// ── Constructor / Destructor ─────────────────────────────────────────

OpenGLVertexArray::OpenGLVertexArray()
{
    DMGE_GL_CALL(glGenVertexArrays(1, &m_RendererID));
}

OpenGLVertexArray::~OpenGLVertexArray()
{
    DMGE_GL_CALL(glDeleteVertexArrays(1, &m_RendererID));
}

// ── Bind / Unbind ────────────────────────────────────────────────────

void OpenGLVertexArray::Bind() const
{
    DMGE_GL_CALL(glBindVertexArray(m_RendererID));
}

void OpenGLVertexArray::Unbind() const
{
    DMGE_GL_CALL(glBindVertexArray(0));
}

// ── Vertex / Index Buffer Attachment ─────────────────────────────────

void OpenGLVertexArray::AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer)
{
    DMGE_CORE_ASSERT(!vertexBuffer->GetLayout().GetElements().empty(),
                     "Vertex buffer has no layout!");

    DMGE_GL_CALL(glBindVertexArray(m_RendererID));
    vertexBuffer->Bind();

    const auto& layout = vertexBuffer->GetLayout();
    for (const auto& element : layout)
    {
        switch (element.Type)
        {
            case ShaderDataType::Float:
            case ShaderDataType::Float2:
            case ShaderDataType::Float3:
            case ShaderDataType::Float4:
            {
                DMGE_GL_CALL(glEnableVertexAttribArray(m_VertexBufferIndex));
                DMGE_GL_CALL(glVertexAttribPointer(m_VertexBufferIndex,
                    element.GetComponentCount(),
                    GL_FLOAT,
                    element.Normalized ? GL_TRUE : GL_FALSE,
                    layout.GetStride(),
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset))));
                if (element.PerInstance)
                    DMGE_GL_CALL(glVertexAttribDivisor(m_VertexBufferIndex, 1));
                m_VertexBufferIndex++;
                break;
            }
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4:
            case ShaderDataType::Bool:
            {
                DMGE_GL_CALL(glEnableVertexAttribArray(m_VertexBufferIndex));
                DMGE_GL_CALL(glVertexAttribIPointer(m_VertexBufferIndex,
                    element.GetComponentCount(),
                    ShaderDataTypeToOpenGLBaseType(element.Type),
                    layout.GetStride(),
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset))));
                if (element.PerInstance)
                    DMGE_GL_CALL(glVertexAttribDivisor(m_VertexBufferIndex, 1));
                m_VertexBufferIndex++;
                break;
            }
            case ShaderDataType::Mat3:
            case ShaderDataType::Mat4:
            {
                uint8_t count = (element.Type == ShaderDataType::Mat3) ? 3 : 4;
                for (uint8_t i = 0; i < count; i++)
                {
                    DMGE_GL_CALL(glEnableVertexAttribArray(m_VertexBufferIndex));
                    DMGE_GL_CALL(glVertexAttribPointer(m_VertexBufferIndex,
                        count,
                        GL_FLOAT,
                        element.Normalized ? GL_TRUE : GL_FALSE,
                        layout.GetStride(),
                        reinterpret_cast<const void*>(
                            static_cast<uintptr_t>(element.Offset + sizeof(float) * 4 * i))));
                    DMGE_GL_CALL(glVertexAttribDivisor(m_VertexBufferIndex, 1));
                    m_VertexBufferIndex++;
                }
                break;
            }
            default:
                DMGE_CORE_ASSERT(false, "Unknown ShaderDataType!");
                break;
        }
    }

    m_VertexBuffers.push_back(vertexBuffer);
}

void OpenGLVertexArray::SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer)
{
    DMGE_GL_CALL(glBindVertexArray(m_RendererID));
    indexBuffer->Bind();

    m_IndexBuffer = indexBuffer;
}

} // namespace DMGameEngine