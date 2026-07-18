/*
 * DMGameEngine - Vulkan Texture Helpers
 *
 * Inline helpers shared by VulkanTexture2D / VulkanTexture2DArray /
 * VulkanTextureCube: image + view + sampler creation, layout
 * transitions, staging upload and mipmap generation via blits.
 */

#pragma once

#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTextureUtils.h"
#include "DMGameEngine/Renderer/Texture.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstring>
#include <cstdint>

namespace DMGameEngine::Detail {

inline VkImageAspectFlags FormatAspect(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::Depth:        return VK_IMAGE_ASPECT_DEPTH_BIT;
        case TextureFormat::DepthStencil: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:                          return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

// Create a GPU-local image (allocated via VMA).
inline void CreateImage(VkImageType imageType, VkFormat format,
                        uint32_t width, uint32_t height,
                        uint32_t mipLevels, uint32_t arrayLayers,
                        VkImageCreateFlags flags, VkImageUsageFlags usage,
                        VkImage& outImage, VmaAllocation& outAlloc)
{
    auto& dev = VulkanDevice::Get();

    VkImageCreateInfo info{};
    info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags                 = flags;
    info.imageType             = imageType;
    info.format                = format;
    info.extent                = { width, height, 1 };
    info.mipLevels             = mipLevels;
    info.arrayLayers           = arrayLayers;
    info.samples               = VK_SAMPLE_COUNT_1_BIT;
    info.tiling                = VK_IMAGE_TILING_OPTIMAL;
    info.usage                 = usage;
    info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(dev.Allocator, &info, &allocInfo, &outImage, &outAlloc, nullptr));
}

inline VkImageView CreateImageView(VkImage image, VkImageViewType viewType,
                                    VkFormat format, VkImageAspectFlags aspect,
                                    uint32_t mipLevels, uint32_t layerCount)
{
    auto& dev = VulkanDevice::Get();
    VkImageViewCreateInfo info{};
    info.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image      = image;
    info.viewType   = viewType;
    info.format     = format;
    info.subresourceRange.aspectMask     = aspect;
    info.subresourceRange.baseMipLevel   = 0;
    info.subresourceRange.levelCount     = mipLevels;
    info.subresourceRange.baseArrayLayer  = 0;
    info.subresourceRange.layerCount      = layerCount;

    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(dev.Device, &info, nullptr, &view));
    return view;
}

inline VkSampler CreateSampler(TextureFilter minFilter, TextureFilter magFilter,
                               TextureWrap wrapS, TextureWrap wrapT,
                               TextureWrap wrapR, bool mipmaps, uint32_t mipLevels,
                               bool anisotropySupported, float maxAniso)
{
    auto& dev = VulkanDevice::Get();
    VkSamplerCreateInfo info{};
    info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter    = TextureFilterToVk(magFilter);
    info.minFilter    = TextureFilterToVk(minFilter);
    info.mipmapMode   = mipmaps ? TextureFilterToMipmapMode(minFilter) : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = TextureWrapToVk(wrapS);
    info.addressModeV = TextureWrapToVk(wrapT);
    info.addressModeW = TextureWrapToVk(wrapR);
    info.mipLodBias   = 0.0f;
    info.anisotropyEnable = anisotropySupported ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy    = anisotropySupported ? maxAniso : 1.0f;
    info.compareEnable    = VK_FALSE;
    info.compareOp        = VK_COMPARE_OP_ALWAYS;
    info.minLod           = 0.0f;
    info.maxLod           = mipmaps ? static_cast<float>(mipLevels) : 0.0f;
    info.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(dev.Device, &info, nullptr, &sampler));
    return sampler;
}

inline void TransitionLayout(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkImageAspectFlags aspect, uint32_t mipLevels, uint32_t layerCount,
                             VkPipelineStageFlags srcStage, VkAccessFlags srcAccess,
                             VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout            = oldLayout;
    barrier.newLayout            = newLayout;
    barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                 = image;
    barrier.subresourceRange.aspectMask     = aspect;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = mipLevels;
    barrier.subresourceRange.baseArrayLayer  = 0;
    barrier.subresourceRange.layerCount      = layerCount;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
}

// Upload pixel data for one layer / mip level via a staging buffer.
inline void UploadToImage(VkImage image, VkFormat format, uint32_t width, uint32_t height,
                           const void* data, uint32_t layer, TextureFormat srcFormat)
{
    auto& dev = VulkanDevice::Get();
    VkDeviceSize pixelSize = TextureFormatPixelSize(srcFormat);
    VkDeviceSize size = pixelSize * width * height;

    // Staging buffer.
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging; VmaAllocation stagingAlloc; VmaAllocationInfo allocResult{};
    VK_CHECK(vmaCreateBuffer(dev.Allocator, &bufInfo, &allocInfo,
                              &staging, &stagingAlloc, &allocResult));
    std::memcpy(allocResult.pMappedData, data, size);

    VkImageAspectFlags aspect = FormatAspect(srcFormat);

    dev.ImmediateSubmit([&](VkCommandBuffer cmd) {
        TransitionLayout(cmd, image,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         aspect, 1, 1,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkBufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask     = aspect;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer  = layer;
        region.imageSubresource.layerCount      = 1;
        region.imageOffset      = { 0, 0, 0 };
        region.imageExtent      = { width, height, 1 };

        vkCmdCopyBufferToImage(cmd, staging, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });

    vmaDestroyBuffer(dev.Allocator, staging, stagingAlloc);
}

// Transition the whole image (all layers/mips) to SHADER_READ_ONLY_OPTIMAL.
inline void TransitionToShaderReadOnly(VkImage image, VkImageAspectFlags aspect,
                                        uint32_t mipLevels, uint32_t layerCount)
{
    auto& dev = VulkanDevice::Get();
    dev.ImmediateSubmit([&](VkCommandBuffer cmd) {
        TransitionLayout(cmd, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         aspect, mipLevels, layerCount,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    });
}

// Generate the mip chain from the base level via blits, leaving every
// level in SHADER_READ_ONLY_OPTIMAL. No-op when mipLevels == 1.
inline void GenerateMipmaps(VkImage image, VkFormat format,
                             uint32_t width, uint32_t height,
                             uint32_t mipLevels, uint32_t layerCount,
                             TextureFilter filter)
{
    if (mipLevels <= 1)
        return;

    auto& dev = VulkanDevice::Get();
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(dev.PhysicalDevice, format, &props);
    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
        !(props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
    {
        // Format cannot blit; leave the base level and transition.
        TransitionToShaderReadOnly(image, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layerCount);
        return;
    }

    VkFilter vkFilter = TextureFilterToVk(filter);

    dev.ImmediateSubmit([&](VkCommandBuffer cmd) {
        // Base level -> TRANSFER_SRC; levels 1..n-1 -> TRANSFER_DST.
        {
            VkImageMemoryBarrier b0{};
            b0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b0.srcQueueFamilyIndex = b0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b0.image = image;
            b0.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            b0.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b0.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            b0.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            b0.subresourceRange.baseMipLevel    = 0;
            b0.subresourceRange.levelCount      = 1;
            b0.subresourceRange.baseArrayLayer   = 0;
            b0.subresourceRange.layerCount       = layerCount;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &b0);
        }

        for (uint32_t i = 1; i < mipLevels; ++i)
        {
            VkImageMemoryBarrier toDst{};
            toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toDst.srcQueueFamilyIndex = toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toDst.image = image;
            toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toDst.srcAccessMask = 0;
            toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toDst.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            toDst.subresourceRange.baseMipLevel     = i;
            toDst.subresourceRange.levelCount       = 1;
            toDst.subresourceRange.baseArrayLayer   = 0;
            toDst.subresourceRange.layerCount        = layerCount;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &toDst);

            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel    = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount  = layerCount;
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { static_cast<int32_t>(width >> (i - 1) ? width >> (i - 1) : 1),
                                   static_cast<int32_t>(height >> (i - 1) ? height >> (i - 1) : 1), 1 };

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel    = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount  = layerCount;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { static_cast<int32_t>(width >> i ? width >> i : 1),
                                   static_cast<int32_t>(height >> i ? height >> i : 1), 1 };

            vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, vkFilter);

            // Promote level i-1 -> SHADER_READ_ONLY (already fully blitted).
            VkImageMemoryBarrier toShader{};
            toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toShader.srcQueueFamilyIndex = toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShader.image = image;
            toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toShader.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toShader.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            toShader.subresourceRange.baseMipLevel     = i - 1;
            toShader.subresourceRange.levelCount       = 1;
            toShader.subresourceRange.baseArrayLayer   = 0;
            toShader.subresourceRange.layerCount        = layerCount;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &toShader);

            // Promote level i -> TRANSFER_SRC for the next iteration (unless last).
            if (i < mipLevels - 1)
            {
                VkImageMemoryBarrier toSrc{};
                toSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                toSrc.srcQueueFamilyIndex = toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toSrc.image = image;
                toSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                toSrc.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                toSrc.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
                toSrc.subresourceRange.baseMipLevel     = i;
                toSrc.subresourceRange.levelCount       = 1;
                toSrc.subresourceRange.baseArrayLayer   = 0;
                toSrc.subresourceRange.layerCount        = layerCount;
                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &toSrc);
            }
        }

        // Final level (mipLevels-1) is still TRANSFER_DST -> promote to SHADER_READ_ONLY.
        VkImageMemoryBarrier finalB{};
        finalB.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        finalB.srcQueueFamilyIndex = finalB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        finalB.image = image;
        finalB.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        finalB.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        finalB.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        finalB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        finalB.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        finalB.subresourceRange.baseMipLevel     = mipLevels - 1;
        finalB.subresourceRange.levelCount       = 1;
        finalB.subresourceRange.baseArrayLayer   = 0;
        finalB.subresourceRange.layerCount        = layerCount;
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &finalB);
    });
}

// After uploading the base level: transition level 0 from TRANSFER_DST and
// any remaining mip levels from UNDEFINED to SHADER_READ_ONLY_OPTIMAL, so
// the whole image is in a valid sampled layout (mip content may be stale
// until GenerateMipmaps() runs).
inline void FinalizeAfterUpload(VkImage image, VkImageAspectFlags aspect,
                                uint32_t mipLevels, uint32_t baseLayer, uint32_t layerCount)
{
    if (mipLevels == 0)
        return;
    auto& dev = VulkanDevice::Get();
    dev.ImmediateSubmit([&](VkCommandBuffer cmd) {
        // Base level: TRANSFER_DST -> SHADER_READ_ONLY.
        {
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = image;
            b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            b.subresourceRange.aspectMask      = aspect;
            b.subresourceRange.baseMipLevel    = 0;
            b.subresourceRange.levelCount      = 1;
            b.subresourceRange.baseArrayLayer   = baseLayer;
            b.subresourceRange.layerCount        = layerCount;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &b);
        }
        if (mipLevels > 1)
        {
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = image;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcAccessMask = 0;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            b.subresourceRange.aspectMask      = aspect;
            b.subresourceRange.baseMipLevel    = 1;
            b.subresourceRange.levelCount      = mipLevels - 1;
            b.subresourceRange.baseArrayLayer   = baseLayer;
            b.subresourceRange.layerCount        = layerCount;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &b);
        }
    });
}

} // namespace DMGameEngine::Detail
