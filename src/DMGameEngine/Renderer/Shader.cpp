/*
 * DMGameEngine - Shader Factory Implementation
 */

#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLShader.h"

namespace DMGameEngine {

std::shared_ptr<Shader> Shader::Create(std::string_view filepath)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return std::make_shared<OpenGLShader>(filepath);

        case Renderer::API::Vulkan:
        case Renderer::API::DirectX:
        case Renderer::API::None:
            DMGE_CORE_ASSERT(false, "Renderer::API not supported yet!");
            return nullptr;
    }

    DMGE_CORE_ASSERT(false, "Unknown Renderer::API!");
    return nullptr;
}

std::shared_ptr<Shader> Shader::Create(std::string_view name,
                                       std::string_view vertexSrc,
                                       std::string_view fragmentSrc)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return std::make_shared<OpenGLShader>(name, vertexSrc, fragmentSrc);

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
