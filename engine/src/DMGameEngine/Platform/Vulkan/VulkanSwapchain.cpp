/*
 * DMGameEngine - Vulkan Swapchain Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanSwapchain.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTextureUtils.h"

#include <algorithm>
#include <climits>

namespace DMGameEngine {

using Detail::TextureFormatToVk; // (kept for potential future format reuse)

// ── Lifetime ─────────────────────────────────────────────────────

VulkanSwapchain::~VulkanSwapchain()
{
    Cleanup();
}

void VulkanSwapchain::Init(VkInstance instance, VkSurfaceKHR surface, uint32_t width, uint32_t height)
{
    m_Instance = instance;
    m_Surface  = surface;
    CreateSwapchain(width, height);
    CreateImageViews();
    CreateDepthResources();
}

void VulkanSwapchain::Cleanup()
{
    auto& dev = VulkanDevice::Get();

    if (m_DepthView != VK_NULL_HANDLE)
        vkDestroyImageView(dev.Device, m_DepthView, nullptr);
    if (m_DepthImage != VK_NULL_HANDLE)
        vmaDestroyImage(dev.Allocator, m_DepthImage, m_DepthAlloc);
    m_DepthView = VK_NULL_HANDLE;
    m_DepthImage = VK_NULL_HANDLE;
    m_DepthAlloc = VK_NULL_HANDLE;

    for (auto view : m_ImageViews)
        vkDestroyImageView(dev.Device, view, nullptr);
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(dev.Device, m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

void VulkanSwapchain::Recreate(uint32_t width, uint32_t height)
{
    auto& dev = VulkanDevice::Get();

    // Wait for the device to be idle before tearing down swapchain resources.
    vkDeviceWaitIdle(dev.Device);

    Cleanup();
    CreateSwapchain(width, height);
    CreateImageViews();
    CreateDepthResources();
}

// ── Surface support queries ──────────────────────────────────────

VulkanSwapchain::SwapchainSupportDetails VulkanSwapchain::QuerySupport() const
{
    auto& dev = VulkanDevice::Get();
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev.PhysicalDevice, m_Surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev.PhysicalDevice, m_Surface, &formatCount, nullptr);
    if (formatCount > 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev.PhysicalDevice, m_Surface, &formatCount, details.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev.PhysicalDevice, m_Surface, &modeCount, nullptr);
    if (modeCount > 0)
    {
        details.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev.PhysicalDevice, m_Surface, &modeCount, details.presentModes.data());
    }
    return details;
}

VkSurfaceFormatKHR
VulkanSwapchain::ChooseFormat(const std::vector<VkSurfaceFormatKHR>& available) const
{
    for (const auto& f : available)
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return available.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                             : available.front();
}

VkPresentModeKHR
VulkanSwapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& available) const
{
    for (auto m : available)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
VulkanSwapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const
{
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;

    VkExtent2D actual;
    actual.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
    actual.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    return actual;
}

// ── Swapchain creation ────────────────────────────────────────────

void VulkanSwapchain::CreateSwapchain(uint32_t width, uint32_t height)
{
    auto& dev = VulkanDevice::Get();
    auto support = QuerySupport();

    auto format       = ChooseFormat(support.formats);
    auto presentMode  = ChoosePresentMode(support.presentModes);
    auto extent       = ChooseExtent(support.capabilities, width, height);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = m_Surface;
    info.minImageCount    = imageCount;
    info.imageFormat      = format.format;
    info.imageColorSpace  = format.colorSpace;
    info.imageExtent      = extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform     = support.capabilities.currentTransform;
    info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode       = presentMode;
    info.clipped          = VK_TRUE;
    info.oldSwapchain     = VK_NULL_HANDLE;

    uint32_t families[] = { dev.GraphicsFamily, dev.PresentFamily };
    if (dev.GraphicsFamily != dev.PresentFamily)
    {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices    = families;
    }
    else
    {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(dev.Device, &info, nullptr, &m_Swapchain));

    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(dev.Device, m_Swapchain, &actualCount, nullptr);
    m_Images.resize(actualCount);
    vkGetSwapchainImagesKHR(dev.Device, m_Swapchain, &actualCount, m_Images.data());

    m_ImageFormat = format.format;
    m_Extent      = extent;
}

void VulkanSwapchain::CreateImageViews()
{
    auto& dev = VulkanDevice::Get();
    m_ImageViews.resize(m_Images.size());

    for (size_t i = 0; i < m_Images.size(); ++i)
    {
        VkImageViewCreateInfo info{};
        info.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image      = m_Images[i];
        info.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        info.format     = m_ImageFormat;
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer  = 0;
        info.subresourceRange.layerCount      = 1;
        VK_CHECK(vkCreateImageView(dev.Device, &info, nullptr, &m_ImageViews[i]));
    }
}

void VulkanSwapchain::CreateDepthResources()
{
    auto& dev = VulkanDevice::Get();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = m_DepthFormat;
    imageInfo.extent        = { m_Extent.width, m_Extent.height, 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling         = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage          = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(dev.Allocator, &imageInfo, &allocInfo,
                            &m_DepthImage, &m_DepthAlloc, nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image       = m_DepthImage;
    viewInfo.viewType    = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format      = m_DepthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer  = 0;
    viewInfo.subresourceRange.layerCount      = 1;
    VK_CHECK(vkCreateImageView(dev.Device, &viewInfo, nullptr, &m_DepthView));
}

// ── Acquire / Present ─────────────────────────────────────────────

VkResult VulkanSwapchain::AcquireNextImage(VkSemaphore imageAvailable, uint32_t* imageIndex)
{
    return vkAcquireNextImageKHR(VulkanDevice::Get().Device, m_Swapchain,
                                 UINT64_MAX, imageAvailable, VK_NULL_HANDLE, imageIndex);
}

VkResult VulkanSwapchain::Present(VkSemaphore renderFinished, uint32_t imageIndex)
{
    VkPresentInfoKHR info{};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &renderFinished;
    info.swapchainCount     = 1;
    info.pSwapchains        = &m_Swapchain;
    info.pImageIndices      = &imageIndex;
    return vkQueuePresentKHR(VulkanDevice::Get().PresentQueue, &info);
}

} // namespace DMGameEngine
