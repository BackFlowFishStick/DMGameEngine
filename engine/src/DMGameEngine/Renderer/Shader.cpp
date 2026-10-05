/*
 * DMGameEngine - Shader Factory Implementation
 */

#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLShader.h"
#ifdef DMGE_D3D11
#include "DMGameEngine/Platform/DirectX/D3D11Shader.h"
#endif
#include "DMGameEngine/Asset/AssetManager.h"  // ShaderLibrary::Load delegates to AssetManager
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanShader.h"
#endif

namespace DMGameEngine {

DM::Ref<Shader> Shader::Create(std::string_view filepath)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLShader>(filepath);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanShader>(filepath);
#else
            DMGE_CORE_ASSERT(false, "Vulkan backend not built (enable DMGE_VULKAN_BACKEND).");
            return nullptr;
#endif

        case Renderer::API::DirectX:
#ifdef DMGE_D3D11
            return DM::CreateRef<D3D11Shader>(filepath);
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

DM::Ref<Shader> Shader::Create(std::string_view name,
                                       std::string_view vertexSrc,
                                       std::string_view fragmentSrc)
{
    switch (Renderer::GetAPI())
    {
        case Renderer::API::OpenGL:
            return DM::CreateRef<OpenGLShader>(name, vertexSrc, fragmentSrc);

        case Renderer::API::Vulkan:
#ifdef DMGE_VULKAN
            return DM::CreateRef<VulkanShader>(name, vertexSrc, fragmentSrc);
#else
            DMGE_CORE_ASSERT(false, "Vulkan backend not built (enable DMGE_VULKAN_BACKEND).");
            return nullptr;
#endif

        case Renderer::API::DirectX:
#ifdef DMGE_D3D11
            return DM::CreateRef<D3D11Shader>(name, vertexSrc, fragmentSrc);
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

// -- Shader Library Implementation ---

namespace {

std::string ExtractShaderName(const std::string& filepath)
{
    auto lastSlash = filepath.find_last_of("/\\");
    auto lastDot   = filepath.rfind('.');
    auto start     = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    auto count     = (lastDot == std::string::npos || lastDot < start)
                         ? std::string::npos
                         : lastDot - start;
    return filepath.substr(start, count);
}

} // namespace

void ShaderLibrary::Add(const DM::Ref<Shader>& shader)
{
    Add(shader->GetName(), shader);
}

void ShaderLibrary::Add(const std::string& name, const DM::Ref<Shader>& shader)
{
    DMGE_CORE_ASSERT(shader, "ShaderLibrary::Add - shader is null!");
    DMGE_CORE_ASSERT(!Exists(name), "Shader '{0}' already exists!", name);
    m_Shaders[name] = shader;
}

DM::Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
{
    return Load(ExtractShaderName(filepath), filepath);
}

DM::Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
{
    if (Exists(name))
        return Get(name);

    // Delegate to AssetManager for dedup + cache (UUID-based). ShaderLibrary
    // keeps the name index (name -> Ref<Shader>); AssetManager owns the resource
    // cache (UUID -> weak_ptr) so the same file loads once across the engine.
    auto shader = AssetManager::Get().Load<Shader>(filepath);
    Add(name, shader);
    return shader;
}

DM::Ref<Shader> ShaderLibrary::Get(const std::string& name) const
{
    auto it = m_Shaders.find(name);
    if (it != m_Shaders.end())
        return it->second;

    DMGE_CORE_ASSERT(false, "Shader '{0}' not found in library!", name);
    return nullptr;
}

bool ShaderLibrary::Exists(const std::string& name) const
{
    return m_Shaders.find(name) != m_Shaders.end();
}

} // namespace DMGameEngine
