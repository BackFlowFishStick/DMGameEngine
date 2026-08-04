/*
 * DMGameEngine - Vulkan Device
 *
 * Central Vulkan device management shared by every Vulkan backend
 * resource. Owns the logical VkDevice, the graphics queue + family, a
 * transfer command pool, and a VMA allocator for buffer/image memory.
 *
 * The VulkanGraphicsContext owns the lifetime; resources reach the
 * device through the static VulkanDevice::Get() accessor, mirroring how
 * OpenGL resources implicitly share the global current context.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <functional>

namespace DMGameEngine {

class DMGE_API VulkanDevice
{
public:
    struct QueueFamilyIndices
    {
        uint32_t Graphics = ~0u;
        uint32_t Present  = ~0u;
        bool IsComplete() const { return Graphics != ~0u && Present != ~0u; }
    };

    // instance + surface are created by VulkanGraphicsContext (surface needs
    // the window). The device picks a physical device that can both render
    // and present to that surface, then creates the logical device.
    static void Init(VkInstance instance, VkSurfaceKHR surface);
    static void Shutdown();
    static VulkanDevice& Get();

    // Records and submits a one-time command buffer on the graphics queue,
    // waiting on a fence until it completes. Used for staging uploads
    // (buffer/image data, layout transitions).
    void ImmediateSubmit(const std::function<void(VkCommandBuffer)>& fn);

    // ── Handles ─────────────────────────────────────────────────
    VkPhysicalDevice       PhysicalDevice  = VK_NULL_HANDLE;
    VkDevice               Device          = VK_NULL_HANDLE;
    VkPhysicalDeviceType   DeviceType      = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    VkQueue                GraphicsQueue   = VK_NULL_HANDLE;
    VkQueue                PresentQueue    = VK_NULL_HANDLE;
    uint32_t               GraphicsFamily  = ~0u;
    uint32_t               PresentFamily    = ~0u;
    VkCommandPool          CommandPool      = VK_NULL_HANDLE;
    VmaAllocator           Allocator        = VK_NULL_HANDLE;

    // Cached limits / properties used by other backends.
    VkPhysicalDeviceProperties       Properties{};
    VkPhysicalDeviceMemoryProperties MemoryProperties{};
    VkSampleCountFlags               MaxUsableSampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkDeviceSize                     MinUniformBufferOffsetAlignment = 16;

    // 1.3 dynamic-rendering + sync2 are enabled at device creation.
    bool DynamicRenderingEnabled = false;
    bool Synchronization2Enabled = false;

private:
    VulkanDevice() = default;
    ~VulkanDevice();
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    void CreateInternal(VkInstance instance, VkSurfaceKHR surface);
    void PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void CreateLogicalDevice(VkSurfaceKHR surface);
    void CreateCommandPool();
    void CreateAllocator(VkInstance instance);

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice dev) const;
    bool IsDeviceSuitable(VkPhysicalDevice dev, VkSurfaceKHR surface) const;

    // Immediate-submit transient resources.
    VkCommandBuffer m_ImmediateCmd = VK_NULL_HANDLE;
    VkFence         m_ImmediateFence = VK_NULL_HANDLE;

    static VulkanDevice* s_Instance;
};

} // namespace DMGameEngine
