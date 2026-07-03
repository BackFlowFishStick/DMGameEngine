/*
 * DMGameEngine - Vertex Buffer Factory Implementation
 */

#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

#include "DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.h"

namespace DMGameEngine {

std::shared_ptr<VertexBuffer> VertexBuffer::Create(uint32_t size)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return std::make_shared<OpenGLVertexBuffer>(size);

        case Renderer::API::Vulkan:
        case Renderer::API::DirectX:
        case Renderer::API::None:
            DMGE_CORE_ASSERT(false, "Renderer::API not supported yet!");
            return nullptr;
    }

    DMGE_CORE_ASSERT(false, "Unknown Renderer::API!");
    return nullptr;
}

std::shared_ptr<VertexBuffer> VertexBuffer::Create(const void* vertices, uint32_t size)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return std::make_shared<OpenGLVertexBuffer>(vertices, size);

        case Renderer::API::Vulkan:
        case Renderer::API::DirectX:
        case Renderer::API::None:
            DMGE_CORE_ASSERT(false, "Renderer::API not supported yet!");
            return nullptr;
    }

    DMGE_CORE_ASSERT(false, "Unknown Renderer::API!");
    return nullptr;
}

} // namespace DMGameEngine
