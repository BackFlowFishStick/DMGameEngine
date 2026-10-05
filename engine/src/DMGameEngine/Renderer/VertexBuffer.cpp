/*
 * DMGameEngine - Vertex Buffer Factory Implementation
 */

#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

#include "DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanVertexBuffer.h"
#endif
#ifdef DMGE_D3D11
#include "DMGameEngine/Platform/DirectX/D3D11VertexBuffer.h"
#endif

namespace DMGameEngine {

DM::Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLVertexBuffer>(size);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanVertexBuffer>(size);
#else
            DMGE_CORE_ASSERT(false, "Vulkan backend not built (enable DMGE_VULKAN_BACKEND).");
            return nullptr;
#endif

        case Renderer::API::DirectX:
#ifdef DMGE_D3D11
            return DM::CreateRef<D3D11VertexBuffer>(size);
#else
            DMGE_CORE_ASSERT(false, "DirectX backend not built (enable DMGE_D3D11).");
            return nullptr;
#endif

        case Renderer::API::None:
            DMGE_CORE_ASSERT(false, "Renderer::API not supported yet!");
            return nullptr;
    }

    DMGE_CORE_ASSERT(false, "Unknown Renderer::API!");
    return nullptr;
}

DM::Ref<VertexBuffer> VertexBuffer::Create(const void* vertices, uint32_t size)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLVertexBuffer>(vertices, size);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanVertexBuffer>(vertices, size);
#else
            DMGE_CORE_ASSERT(false, "Vulkan backend not built (enable DMGE_VULKAN_BACKEND).");
            return nullptr;
#endif

        case Renderer::API::DirectX:
#ifdef DMGE_D3D11
            return DM::CreateRef<D3D11VertexBuffer>(vertices, size);
#else
            DMGE_CORE_ASSERT(false, "DirectX backend not built (enable DMGE_D3D11).");
            return nullptr;
#endif

        case Renderer::API::None:
            DMGE_CORE_ASSERT(false, "Renderer::API not supported yet!");
            return nullptr;
    }

    DMGE_CORE_ASSERT(false, "Unknown Renderer::API!");
    return nullptr;
}

} // namespace DMGameEngine
