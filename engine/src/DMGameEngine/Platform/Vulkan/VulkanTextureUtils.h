/*
 * DMGameEngine - Vulkan Texture / Vertex Utilities
 *
 * Shared Vk enum-mapping helpers used by every Vulkan backend
 * (VulkanTexture2D, VulkanTextureCube, VulkanTexture2DArray,
 *  VulkanVertexArray). Header-only and inline so each backend picks
 *  up the mappings without an extra TU.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture.h"   // TextureFormat / Filter / Wrap
#include "DMGameEngine/Renderer/Shader.h"    // ShaderDataType
#include "DMGameEngine/Core/Log.h"          // DMGE_CORE_ASSERT

#include <vulkan/vulkan.h>
#include <cstdint>

namespace DMGameEngine::Detail {

// TextureFormat -> Vulkan image format.
inline VkFormat TextureFormatToVk(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:           return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8:          return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8:         return VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::RGBA8:        return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::R16F:         return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::RG16F:        return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGB16F:       return VK_FORMAT_R16G16B16_SFLOAT;
        case TextureFormat::RGBA16F:      return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32F:         return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32F:        return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGB32F:       return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32F:      return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::Depth:        return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::DepthStencil: return VK_FORMAT_D24_UNORM_S8_UINT;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFormat!");
            return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

// Number of bytes per texel for a format (for staging-buffer sizing).
inline uint32_t TextureFormatPixelSize(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:           return 1;
        case TextureFormat::RG8:          return 2;
        case TextureFormat::RGB8:         return 3;
        case TextureFormat::RGBA8:        return 4;
        case TextureFormat::R16F:         return 2;
        case TextureFormat::RG16F:        return 4;
        case TextureFormat::RGB16F:       return 6;
        case TextureFormat::RGBA16F:      return 8;
        case TextureFormat::R32F:         return 4;
        case TextureFormat::RG32F:        return 8;
        case TextureFormat::RGB32F:       return 12;
        case TextureFormat::RGBA32F:      return 16;
        case TextureFormat::Depth:        return 4;
        case TextureFormat::DepthStencil: return 4;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFormat!");
            return 4;
    }
}

inline VkFilter TextureFilterToVk(TextureFilter filter)
{
    switch (filter)
    {
        case TextureFilter::Nearest: return VK_FILTER_NEAREST;
        case TextureFilter::Linear:  return VK_FILTER_LINEAR;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureFilter!");
            return VK_FILTER_LINEAR;
    }
}

// Mipmap mode for the min filter (linear = trilinear/aniso between levels).
inline VkSamplerMipmapMode TextureFilterToMipmapMode(TextureFilter filter)
{
    switch (filter)
    {
        case TextureFilter::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case TextureFilter::Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

inline VkSamplerAddressMode TextureWrapToVk(TextureWrap wrap)
{
    switch (wrap)
    {
        case TextureWrap::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureWrap::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case TextureWrap::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        default:
            DMGE_CORE_ASSERT(false, "Unknown TextureWrap!");
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

// ShaderDataType -> Vulkan vertex attribute format.
inline VkFormat ShaderDataTypeToVkFormat(ShaderDataType type)
{
    switch (type)
    {
        case ShaderDataType::Float:  return VK_FORMAT_R32_SFLOAT;
        case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case ShaderDataType::Int:    return VK_FORMAT_R32_SINT;
        case ShaderDataType::Int2:   return VK_FORMAT_R32G32_SINT;
        case ShaderDataType::Int3:   return VK_FORMAT_R32G32B32_SINT;
        case ShaderDataType::Int4:   return VK_FORMAT_R32G32B32A32_SINT;
        case ShaderDataType::Bool:   return VK_FORMAT_R8_UINT;
        case ShaderDataType::Mat3:   return VK_FORMAT_R32G32B32_SFLOAT; // per-column
        case ShaderDataType::Mat4:   return VK_FORMAT_R32G32B32A32_SFLOAT; // per-column
        default:
            DMGE_CORE_ASSERT(false, "Unknown ShaderDataType!");
            return VK_FORMAT_UNDEFINED;
    }
}

// Number of mipmap levels needed for a texture of the given dimensions
// (chain depth: floor(log2(max(w,h))) + 1). Used to size image storage;
// callers pass 1 when mipmaps are disabled.
inline uint32_t MipLevelCount(uint32_t width, uint32_t height)
{
    uint32_t maxDim = (width > height) ? width : height;
    uint32_t levels = 1;
    while (maxDim > 1) { maxDim >>= 1; ++levels; }
    return levels;
}

} // namespace DMGameEngine::Detail
