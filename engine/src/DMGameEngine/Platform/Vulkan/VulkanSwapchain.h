/*
 * DMGameEngine - Vulkan Swapchain
 *
 * Owns the VkSurfaceKHR (created from the GLFW window by the context),
 * the VkSwapchainKHR, its color images + views, and a matching depth
 * image used for depth testing during dynamic rendering.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>

namespace DMGameEngine {

class DMGE_API VulkanSwapchain
{
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain();

    void Init(VkInstance instance, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void Recreate(uint32_t width, uint32_t height);
    void Cleanup();

    // Acquire the next swapchain image. Returns the VkResult so the caller
    // can detect VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR for resize.
    VkResult AcquireNextImage(VkSemaphore imageAvailable, uint32_t* imageIndex);

    // Present the image at imageIndex once renderFinished is signalled.
    VkResult Present(VkSemaphore renderFinished, uint32_t imageIndex);

    VkFormat GetImageFormat()   const { return m_ImageFormat; }
    VkFormat GetDepthFormat()   const { return m_DepthFormat; }
    VkExtent2D GetExtent()      const { return m_Extent; }
    uint32_t   GetImageCount()  const { return static_cast<uint32_t>(m_Images.size()); }
    VkImage    GetImage(uint32_t i) const { return m_Images[i]; }
    VkImageView GetImageView(uint32_t i) const { return m_ImageViews[i]; }
    VkImageView GetDepthView()   const { return m_DepthView; }
    VkImage    GetDepthImage()  const { return m_DepthImage; }

private:
    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>    presentModes;
    };

    SwapchainSupportDetails QuerySupport() const;
    VkSurfaceFormatKHR      ChooseFormat(const std::vector<VkSurfaceFormatKHR>& available) const;
    VkPresentModeKHR        ChoosePresentMode(const std::vector<VkPresentModeKHR>& available) const;
    VkExtent2D              ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const;
    void                    CreateSwapchain(uint32_t width, uint32_t height);
    void                    CreateImageViews();
    void                    CreateDepthResources();

    VkInstance       m_Instance    = VK_NULL_HANDLE;
    VkSurfaceKHR     m_Surface     = VK_NULL_HANDLE;
    VkSwapchainKHR   m_Swapchain    = VK_NULL_HANDLE;
    std::vector<VkImage>       m_Images;
    std::vector<VkImageView>   m_ImageViews;
    VkFormat         m_ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D       m_Extent      = {0, 0};

    // Depth attachment (recreated with the swapchain).
    VkImage          m_DepthImage   = VK_NULL_HANDLE;
    VkImageView      m_DepthView    = VK_NULL_HANDLE;
    VmaAllocation    m_DepthAlloc   = VK_NULL_HANDLE;
    VkFormat         m_DepthFormat  = VK_FORMAT_D32_SFLOAT;
};

} // namespace DMGameEngine
