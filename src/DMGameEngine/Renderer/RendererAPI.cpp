/*
 * DMGameEngine - Renderer API Factory Implementation
 */

#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

#include "DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"

namespace DMGameEngine {

DM::Scope<RendererAPI> RendererAPI::Create()
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateScope<OpenGLRendererAPI>();

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