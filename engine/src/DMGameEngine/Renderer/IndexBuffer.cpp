/*
 * DMGameEngine - Index Buffer Factory Implementation
 */

#include "DMGameEngine/Renderer/IndexBuffer.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

#include "DMGameEngine/Platform/OpenGL/OpenGLIndexBuffer.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanIndexBuffer.h"
#endif

namespace DMGameEngine {

DM::Ref<IndexBuffer> IndexBuffer::Create(const uint32_t* indices, uint32_t count)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLIndexBuffer>(indices, count);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanIndexBuffer>(indices, count);
#else
            DMGE_CORE_ASSERT(false, "Vulkan backend not built (enable DMGE_VULKAN_BACKEND).");
            return nullptr;
#endif

        case Renderer::API::DirectX:
        case Renderer::API::None:
            DMGE_CORE_ASSERT(false, "Renderer::API not supported yet!");
            return nullptr;
    }

    DMGE_CORE_ASSERT(false, "Unknown Renderer::API!");
    return nullptr;
}

} // namespace DMGameEngine
