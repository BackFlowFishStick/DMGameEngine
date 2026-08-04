/*
 * DMGameEngine - Texture2D Factory Implementation
 */

#include "DMGameEngine/Renderer/Texture2D.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLTexture2D.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanTexture2D.h"
#endif

namespace DMGameEngine {

DM::Ref<Texture2D> Texture2D::Create(const Texture2DSpecification& spec)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLTexture2D>(spec);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanTexture2D>(spec);
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

DM::Ref<Texture2D> Texture2D::Create(std::string_view filepath)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLTexture2D>(filepath);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanTexture2D>(filepath);
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
