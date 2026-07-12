/*
 * DMGameEngine - Vertex Array Factory Implementation
 */

#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Core/Log.h"

#include "DMGameEngine/Platform/OpenGL/OpenGLVertexArray.h"

namespace DMGameEngine {

DM::Ref<VertexArray> VertexArray::Create()
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLVertexArray>();

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