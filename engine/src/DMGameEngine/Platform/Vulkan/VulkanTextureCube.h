/*
 * DMGameEngine - Vulkan TextureCube
 *
 * Vulkan implementation of the TextureCube abstraction: a cube-compatible
 * VkImage with 6 layers, one per face.
 */

#pragma once

#include "DMGameEngine/Renderer/TextureCube.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTexture.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <array>
#include <string>
#include <cstdint>

namespace DMGameEngine {

class DMGE_API VulkanTextureCube : public TextureCube, public VulkanTexture
{
public:
    explicit VulkanTextureCube(const TextureCubeSpecification& spec);
    explicit VulkanTextureCube(const std::array<std::string, CubeFaceCount>& facePaths);
    ~VulkanTextureCube() override;

    void Bind(uint32_t slot = 0) const override;
    void Unbind()                 const override {}

    uint32_t GetWidth()      const override { return m_Spec.Size; }
    uint32_t GetHeight()     const override { return m_Spec.Size; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    const TextureCubeSpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size, uint32_t face) override;
    void GenerateMipmaps() override;

    VkSampler   GetVkSampler()   const override { return m_Sampler; }
    VkImageView GetVkImageView() const override { return m_ImageView; }

private:
    void Invalidate();

    VkImage       m_Image     = VK_NULL_HANDLE;
    VmaAllocation m_Alloc      = VK_NULL_HANDLE;
    VkImageView   m_ImageView  = VK_NULL_HANDLE;
    VkSampler     m_Sampler    = VK_NULL_HANDLE;
    uint32_t      m_MipLevels  = 1;
    uint32_t      m_RendererID = 0;
    TextureCubeSpecification m_Spec;
    std::array<std::string, CubeFaceCount> m_FacePaths;

    static uint32_t s_IDCounter;
};

} // namespace DMGameEngine
