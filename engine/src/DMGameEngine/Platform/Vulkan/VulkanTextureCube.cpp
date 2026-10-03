/*
 * DMGameEngine - Vulkan TextureCube Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanTextureCube.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTextureHelpers.h"

#include "DMGameEngine/Core/Log.h"

#include <stb_image.h>

namespace DMGameEngine {

using namespace Detail;

uint32_t VulkanTextureCube::s_IDCounter = 1;

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

VulkanTextureCube::VulkanTextureCube(const TextureCubeSpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

VulkanTextureCube::VulkanTextureCube(const std::array<std::string, CubeFaceCount>& facePaths)
    : m_FacePaths(facePaths)
{
    stbi_set_flip_vertically_on_load(0);
    m_Spec.Format = TextureFormat::RGBA8;

    for (uint32_t i = 0; i < CubeFaceCount; ++i)
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc* data = stbi_load(m_FacePaths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!data)
        {
            DMGE_CORE_ASSERT(false, "Failed to load cube face {0} from file: {1}", i, m_FacePaths[i]);
            continue;
        }
        if (i == 0)
        {
            m_Spec.Size = static_cast<uint32_t>(width);
            Invalidate();
        }
        SetData(data, static_cast<uint32_t>(width * height * 4), i);
        stbi_image_free(data);
    }

    if (m_Image != VK_NULL_HANDLE && m_Spec.GenerateMipmaps)
        GenerateMipmaps();
}

VulkanTextureCube::~VulkanTextureCube()
{
    // Deferred (review item B): an in-flight frame may still sample this
    // texture; destroying here would be use-while-in-flight.
    VulkanDevice::DeferDestroyTexture(m_Sampler, m_ImageView, m_Image, m_Alloc);
    m_Sampler = VK_NULL_HANDLE;
    m_ImageView = VK_NULL_HANDLE;
    m_Image = VK_NULL_HANDLE;
    m_Alloc = nullptr;
}

void VulkanTextureCube::Bind(uint32_t slot) const
{
    if (auto* r = VulkanRendererAPI::Get())
        r->SetBoundTexture(slot, const_cast<VulkanTextureCube*>(this));
}

void VulkanTextureCube::SetData(void* data, uint32_t size, uint32_t face)
{
    (void)size;
    DMGE_CORE_ASSERT(face < CubeFaceCount, "Cube face index out of range!");
    if (m_Image == VK_NULL_HANDLE)
        return;

    UploadToImage(m_Image, TextureFormatToVk(m_Spec.Format),
                  m_Spec.Size, m_Spec.Size, data, face, m_Spec.Format);
    FinalizeAfterUpload(m_Image, FormatAspect(m_Spec.Format), m_MipLevels, face, 1);
}

void VulkanTextureCube::GenerateMipmaps()
{
    if (m_Image == VK_NULL_HANDLE || !m_Spec.GenerateMipmaps)
        return;
    Detail::GenerateMipmaps(m_Image, TextureFormatToVk(m_Spec.Format),
                            m_Spec.Size, m_Spec.Size, m_MipLevels, CubeFaceCount, m_Spec.MinFilter);
}

void VulkanTextureCube::Invalidate()
{
    // Old resources may still be referenced by an in-flight frame: defer
    // instead of destroying (review item B).
    VulkanDevice::DeferDestroyTexture(m_Sampler, m_ImageView, m_Image, m_Alloc);
    m_Sampler = VK_NULL_HANDLE;
    m_ImageView = VK_NULL_HANDLE;
    m_Image = VK_NULL_HANDLE;
    m_Alloc = nullptr;

    VkFormat format = TextureFormatToVk(m_Spec.Format);
    VkImageAspectFlags aspect = FormatAspect(m_Spec.Format);
    m_MipLevels = m_Spec.GenerateMipmaps ? MipLevelCount(m_Spec.Size, m_Spec.Size) : 1;

    CreateImage(VK_IMAGE_TYPE_2D, format, m_Spec.Size, m_Spec.Size,
                m_MipLevels, CubeFaceCount, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                m_Image, m_Alloc);

    m_ImageView = CreateImageView(m_Image, VK_IMAGE_VIEW_TYPE_CUBE, format, aspect,
                                  m_MipLevels, CubeFaceCount);

    float maxAniso = 1.0f;
    bool aniso = AnisotropySupported(maxAniso);
    m_Sampler = CreateSampler(m_Spec.MinFilter, m_Spec.MagFilter,
                              m_Spec.WrapS, m_Spec.WrapT, m_Spec.WrapR,
                              m_Spec.GenerateMipmaps, m_MipLevels, aniso, maxAniso);

    m_RendererID = s_IDCounter++;
}

} // namespace DMGameEngine
