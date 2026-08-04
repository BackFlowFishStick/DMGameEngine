/*
 * DMGameEngine - Vulkan Texture2DArray
 *
 * Vulkan implementation of the Texture2DArray abstraction: a single
 * VkImage holding multiple same-sized 2D layers.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture2DArray.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTexture.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstdint>

namespace DMGameEngine {

class DMGE_API VulkanTexture2DArray : public Texture2DArray, public VulkanTexture
{
public:
    explicit VulkanTexture2DArray(const Texture2DArraySpecification& spec);
    ~VulkanTexture2DArray() override;

    void Bind(uint32_t slot = 0) const override;
    void Unbind()                 const override {}

    uint32_t GetWidth()      const override { return m_Spec.Width;  }
    uint32_t GetHeight()     const override { return m_Spec.Height; }
    uint32_t GetLayerCount() const override { return m_Spec.Layers; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    const Texture2DArraySpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size, uint32_t layer) override;
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
    Texture2DArraySpecification m_Spec;

    static uint32_t s_IDCounter;
};

} // namespace DMGameEngine
