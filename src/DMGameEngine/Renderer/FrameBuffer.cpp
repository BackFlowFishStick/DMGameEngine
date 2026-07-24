/*
 * DMGameEngine - Frame Buffer Factory Implementation
 */

#include "DMGameEngine/Renderer/FrameBuffer.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

#include "DMGameEngine/Platform/OpenGL/OpenGLFrameBuffer.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanFrameBuffer.h"
#endif

namespace DMGameEngine {

DM::Ref<FrameBuffer> FrameBuffer::Create(const FramebufferSpecification& spec)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLFrameBuffer>(spec);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanFrameBuffer>(spec);
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