/*
 * DMGameEngine - Vulkan Texture (shared base)
 *
 * Common interface shared by VulkanTexture2D / VulkanTexture2DArray /
 * VulkanTextureCube so the renderer can write bound samplers into
 * per-draw descriptor sets polymorphically.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

#include <vulkan/vulkan.h>

namespace DMGameEngine {

class DMGE_API VulkanTexture
{
public:
    virtual ~VulkanTexture() = default;

    // Sampler + view used to fill a COMBINED_IMAGE_SAMPLER descriptor.
    virtual VkSampler   GetVkSampler()   const = 0;
    virtual VkImageView GetVkImageView() const = 0;
};

} // namespace DMGameEngine
