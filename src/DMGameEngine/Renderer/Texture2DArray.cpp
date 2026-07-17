/*
 * DMGameEngine - Texture2DArray Factory Implementation
 */

#include "DMGameEngine/Renderer/Texture2DArray.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLTexture2DArray.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanTexture2DArray.h"
#endif

namespace DMGameEngine {

DM::Ref<Texture2DArray> Texture2DArray::Create(const Texture2DArraySpecification& spec)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLTexture2DArray>(spec);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanTexture2DArray>(spec);
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
