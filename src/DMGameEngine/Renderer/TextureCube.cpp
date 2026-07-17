/*
 * DMGameEngine - TextureCube Factory Implementation
 */

#include "DMGameEngine/Renderer/TextureCube.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLTextureCube.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanTextureCube.h"
#endif

namespace DMGameEngine {

DM::Ref<TextureCube> TextureCube::Create(const TextureCubeSpecification& spec)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLTextureCube>(spec);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanTextureCube>(spec);
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

DM::Ref<TextureCube> TextureCube::Create(const std::array<std::string, CubeFaceCount>& facePaths)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLTextureCube>(facePaths);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanTextureCube>(facePaths);
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
