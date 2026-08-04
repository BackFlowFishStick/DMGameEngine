/*
 * DMGameEngine - Vulkan Texture2DArray Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanTexture2DArray.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTextureHelpers.h"

#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

using namespace Detail;

uint32_t VulkanTexture2DArray::s_IDCounter = 1;

namespace {
bool AnisotropySupported(float& maxAniso)
{
    auto& dev = VulkanDevice::Get();
    VkPhysicalDeviceFeatures feats{};
    vkGetPhysicalDeviceFeatures(dev.PhysicalDevice, &feats);
    if (feats.samplerAnisotropy)
    {
        maxAniso = dev.Properties.limits.maxSamplerAnisotropy;
        return true;
    }
    return false;
}
} // anonymous namespace

VulkanTexture2DArray::VulkanTexture2DArray(const Texture2DArraySpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

VulkanTexture2DArray::~VulkanTexture2DArray()
{
    auto& dev = VulkanDevice::Get();
    if (m_Sampler   != VK_NULL_HANDLE) vkDestroySampler(dev.Device, m_Sampler, nullptr);
    if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(dev.Device, m_ImageView, nullptr);
    if (m_Image     != VK_NULL_HANDLE) vmaDestroyImage(dev.Allocator, m_Image, m_Alloc);
}

void VulkanTexture2DArray::Bind(uint32_t slot) const
{
    if (auto* r = VulkanRendererAPI::Get())
        r->SetBoundTexture(slot, const_cast<VulkanTexture2DArray*>(this));
}

void VulkanTexture2DArray::SetData(void* data, uint32_t size, uint32_t layer)
{
    (void)size;
    DMGE_CORE_ASSERT(layer < m_Spec.Layers, "Texture2DArray layer index out of range!");
    if (m_Image == VK_NULL_HANDLE)
        return;

    UploadToImage(m_Image, TextureFormatToVk(m_Spec.Format),
                  m_Spec.Width, m_Spec.Height, data, layer, m_Spec.Format);
    FinalizeAfterUpload(m_Image, FormatAspect(m_Spec.Format), m_MipLevels, layer, 1);
}

void VulkanTexture2DArray::GenerateMipmaps()
{
    if (m_Image == VK_NULL_HANDLE || !m_Spec.GenerateMipmaps)
        return;
    Detail::GenerateMipmaps(m_Image, TextureFormatToVk(m_Spec.Format),
                            m_Spec.Width, m_Spec.Height, m_MipLevels, m_Spec.Layers, m_Spec.MinFilter);
}

void VulkanTexture2DArray::Invalidate()
{
    auto& dev = VulkanDevice::Get();

    if (m_Sampler   != VK_NULL_HANDLE) vkDestroySampler(dev.Device, m_Sampler, nullptr);
    if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(dev.Device, m_ImageView, nullptr);
    if (m_Image     != VK_NULL_HANDLE) vmaDestroyImage(dev.Allocator, m_Image, m_Alloc);

    VkFormat format = TextureFormatToVk(m_Spec.Format);
    VkImageAspectFlags aspect = FormatAspect(m_Spec.Format);
    m_MipLevels = m_Spec.GenerateMipmaps ? MipLevelCount(m_Spec.Width, m_Spec.Height) : 1;

    CreateImage(VK_IMAGE_TYPE_2D, format, m_Spec.Width, m_Spec.Height,
                m_MipLevels, m_Spec.Layers, 0,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                m_Image, m_Alloc);

    m_ImageView = CreateImageView(m_Image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, format, aspect,
                                  m_MipLevels, m_Spec.Layers);

    float maxAniso = 1.0f;
    bool aniso = AnisotropySupported(maxAniso);
    m_Sampler = CreateSampler(m_Spec.MinFilter, m_Spec.MagFilter,
                              m_Spec.WrapS, m_Spec.WrapT, m_Spec.WrapR,
                              m_Spec.GenerateMipmaps, m_MipLevels, aniso, maxAniso);

    m_RendererID = s_IDCounter++;
}

} // namespace DMGameEngine
