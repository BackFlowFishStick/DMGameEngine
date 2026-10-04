/*
 * DMGameEngine - Renderer API Factory Implementation
 */

#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

#include "DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"
#endif
#ifdef DMGE_D3D11
#include "DMGameEngine/Platform/DirectX/DirectXIntegration.h"
#endif

namespace DMGameEngine {

void RendererAPI::Init(const RendererAPIInitConfig& config)
{
    // Apply the requested initial pipeline state through the virtual
    // setters so each backend routes the calls to its own API calls.
    SetClearColor(config.ClearColor);
    SetDepthTest(config.DepthTestEnabled);
    SetDepthFunc(config.DepthFunction);
    SetCullMode(config.Culling);
    SetBlendState(config.BlendEnabled, config.SrcBlendFactor, config.DstBlendFactor);
    SetBlendEquation(config.BlendEquationMode);
}

DM::Scope<RendererAPI> RendererAPI::Create()
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateScope<OpenGLRendererAPI>();

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateScope<VulkanRendererAPI>();
#else
            DMGE_CORE_ASSERT(false, "Vulkan backend not built (enable DMGE_VULKAN_BACKEND).");
            return nullptr;
#endif

        case Renderer::API::DirectX:
#ifdef DMGE_D3D11
            return DirectX::CreateDirectXRendererAPI();
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
