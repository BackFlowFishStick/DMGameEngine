/*
 * DMGameEngine - Texture Factory Implementation
 */

#include "DMGameEngine/Renderer/Texture.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLTexture.h"

namespace DMGameEngine {

DM::Ref<Texture> Texture::Create(const TextureSpecification& spec)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLTexture>(spec);

        case Renderer::API::Vulkan:
        case Renderer::API::DirectX:
        case Renderer::API::None:
            DMGE_CORE_ASSERT(false, "Renderer::API not supported yet!");
            return nullptr;
    }

    DMGE_CORE_ASSERT(false, "Unknown Renderer::API!");
    return nullptr;
}

DM::Ref<Texture> Texture::Create(std::string_view filepath)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLTexture>(filepath);

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
