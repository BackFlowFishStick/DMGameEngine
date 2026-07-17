/*
 * DMGameEngine - Vulkan Texture2D Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanTexture2D.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTextureHelpers.h"

#include "DMGameEngine/Core/Log.h"

#include <stb_image.h>

namespace DMGameEngine {

using namespace Detail;

uint32_t VulkanTexture2D::s_IDCounter = 1;

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

// ── Constructors / Destructor ─────────────────────────────────────

VulkanTexture2D::VulkanTexture2D(const Texture2DSpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

VulkanTexture2D::VulkanTexture2D(std::string_view filepath)
    : m_FilePath(filepath)
{
    stbi_set_flip_vertically_on_load(1);
    int width = 0, height = 0, channels = 0;
    stbi_uc* data = stbi_load(m_FilePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data)
    {
        DMGE_CORE_ASSERT(false, "Failed to load texture from file: {0}", m_FilePath);
        return;
    }
    m_Spec.Width  = static_cast<uint32_t>(width);
    m_Spec.Height = static_cast<uint32_t>(height);
    m_Spec.Format = TextureFormat::RGBA8;

    Invalidate();
    SetData(data, static_cast<uint32_t>(width * height * 4));
    if (m_Spec.GenerateMipmaps)
        GenerateMipmaps();
    stbi_image_free(data);
}

VulkanTexture2D::~VulkanTexture2D()
{
    auto& dev = VulkanDevice::Get();
    if (m_Sampler   != VK_NULL_HANDLE) vkDestroySampler(dev.Device, m_Sampler, nullptr);
    if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(dev.Device, m_ImageView, nullptr);
    if (m_Image     != VK_NULL_HANDLE) vmaDestroyImage(dev.Allocator, m_Image, m_Alloc);
}

// ── Bind ───────────────────────────────────────────────────────────

void VulkanTexture2D::Bind(uint32_t slot) const
{
    if (auto* r = VulkanRendererAPI::Get())
        r->SetBoundTexture(slot, const_cast<VulkanTexture2D*>(this));
}

// ── Data upload ────────────────────────────────────────────────────

void VulkanTexture2D::SetData(void* data, uint32_t size)
{
    (void)size;
    if (m_Image == VK_NULL_HANDLE)
        return;

    UploadToImage(m_Image, TextureFormatToVk(m_Spec.Format),
                  m_Spec.Width, m_Spec.Height, data, 0, m_Spec.Format);
    FinalizeAfterUpload(m_Image, FormatAspect(m_Spec.Format), m_MipLevels, 1);
}

void VulkanTexture2D::GenerateMipmaps()
{
    if (m_Image == VK_NULL_HANDLE || !m_Spec.GenerateMipmaps)
        return;
    Detail::GenerateMipmaps(m_Image, TextureFormatToVk(m_Spec.Format),
                            m_Spec.Width, m_Spec.Height, m_MipLevels, 1, m_Spec.MinFilter);
}

// ── GPU resource creation ─────────────────────────────────────────

void VulkanTexture2D::Invalidate()
{
    auto& dev = VulkanDevice::Get();

    if (m_Sampler   != VK_NULL_HANDLE) vkDestroySampler(dev.Device, m_Sampler, nullptr);
    if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(dev.Device, m_ImageView, nullptr);
    if (m_Image     != VK_NULL_HANDLE) vmaDestroyImage(dev.Allocator, m_Image, m_Alloc);

    VkFormat format = TextureFormatToVk(m_Spec.Format);
    VkImageAspectFlags aspect = FormatAspect(m_Spec.Format);
    m_MipLevels = m_Spec.GenerateMipmaps ? MipLevelCount(m_Spec.Width, m_Spec.Height) : 1;

    CreateImage(VK_IMAGE_TYPE_2D, format, m_Spec.Width, m_Spec.Height,
                m_MipLevels, 1, 0,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                m_Image, m_Alloc);

    m_ImageView = CreateImageView(m_Image, VK_IMAGE_VIEW_TYPE_2D, format, aspect, m_MipLevels, 1);

    float maxAniso = 1.0f;
    bool aniso = AnisotropySupported(maxAniso);
    m_Sampler = CreateSampler(m_Spec.MinFilter, m_Spec.MagFilter,
                              m_Spec.WrapS, m_Spec.WrapT, m_Spec.WrapT,
                              m_Spec.GenerateMipmaps, m_MipLevels, aniso, maxAniso);

    m_RendererID = s_IDCounter++;
}

} // namespace DMGameEngine
