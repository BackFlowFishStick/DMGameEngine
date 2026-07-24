/*
 * DMGameEngine - Vulkan Texture2D
 *
 * Vulkan implementation of the Texture2D abstraction. Creates and
 * manages a VkImage + VkImageView + VkSampler with configurable
 * format, filtering, wrapping and mipmap settings.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture2D.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTexture.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <string>
#include <string_view>
#include <cstdint>

namespace DMGameEngine {

class DMGE_API VulkanTexture2D : public Texture2D, public VulkanTexture
{
public:
    explicit VulkanTexture2D(const Texture2DSpecification& spec);
    explicit VulkanTexture2D(std::string_view filepath);
    // Render-target constructor: same as the spec ctor, but ORs extraUsage
    // into the VkImage usage flags (e.g. COLOR_ATTACHMENT / DEPTH_STENCIL
    // usage) so the texture can be a framebuffer attachment yet remain
    // sampleable. Used by VulkanFrameBuffer.
    VulkanTexture2D(const Texture2DSpecification& spec, VkImageUsageFlags extraUsage);
    ~VulkanTexture2D() override;

    void Bind(uint32_t slot = 0) const override;
    void Unbind()                 const override {}

    uint32_t GetWidth()      const override { return m_Spec.Width;  }
    uint32_t GetHeight()     const override { return m_Spec.Height; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    const Texture2DSpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size) override;
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
    Texture2DSpecification m_Spec;
    VkImageUsageFlags m_ExtraUsage = 0;   // OR-ed into image usage (render-target textures)
    std::string   m_FilePath;

    static uint32_t s_IDCounter;
};

} // namespace DMGameEngine
