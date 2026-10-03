/*
 * DMGameEngine - Vulkan Device Implementation
 */

#define VMA_IMPLEMENTATION

#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"

#include <vector>
#include <set>
#include <cstring>
#include <algorithm>
#include <utility>

namespace DMGameEngine {

VulkanDevice* VulkanDevice::s_Instance = nullptr;

// ── Device extensions we require ─────────────────────────────────
static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

namespace {

// Per-frame deferred-destroy storage (review item B). File-local instead of
// VulkanDevice members: the class is DMGE_API-exported and R1 forbids STL
// containers / mutex in an exported class layout (C4251 + cross-DLL CRT
// layout hazard). The singleton is only instantiated inside this DLL, so
// function-local statics are safe. Mutex because resource destructors may
// run on non-render threads; the flush runs on the frame thread.
std::mutex& DeletionMutex()
{
    static std::mutex s_Mutex;
    return s_Mutex;
}

std::vector<std::function<void()>>& DeletionBucket(uint32_t bucket)
{
    static std::vector<std::function<void()>> s_Queues[Detail::kDeletionBucketCount];
    return s_Queues[bucket % Detail::kDeletionBucketCount];
}

} // anonymous namespace

// ── Lifetime ──────────────────────────────────────────────────────

void VulkanDevice::Init(VkInstance instance, VkSurfaceKHR surface)
{
    DMGE_CORE_ASSERT(!s_Instance, "VulkanDevice already initialized!");
    s_Instance = new VulkanDevice();
    s_Instance->CreateInternal(instance, surface);
}

void VulkanDevice::Shutdown()
{
    if (!s_Instance)
        return;

    auto& d = *s_Instance;

    // Execute any pending deferred destroys first (review item B): their
    // closures reference Device/Allocator below. Shutdown is reached from
    // the graphics-context destructor, which has already idled the device.
    d.FlushAllDeletions();

    if (d.Allocator != VK_NULL_HANDLE)
        vmaDestroyAllocator(d.Allocator);

    if (d.m_ImmediateCmd != VK_NULL_HANDLE)
        vkFreeCommandBuffers(d.Device, d.CommandPool, 1, &d.m_ImmediateCmd);
    if (d.m_ImmediateFence != VK_NULL_HANDLE)
        vkDestroyFence(d.Device, d.m_ImmediateFence, nullptr);

    if (d.CommandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(d.Device, d.CommandPool, nullptr);

    if (d.Device != VK_NULL_HANDLE)
        vkDestroyDevice(d.Device, nullptr);

    delete s_Instance;
    s_Instance = nullptr;
}

VulkanDevice& VulkanDevice::Get()
{
    DMGE_CORE_ASSERT(s_Instance, "VulkanDevice not initialized!");
    return *s_Instance;
}

VulkanDevice::~VulkanDevice()
{
    // Cleanup handled in Shutdown().
}

// ── Internal creation ─────────────────────────────────────────────

void VulkanDevice::CreateInternal(VkInstance instance, VkSurfaceKHR surface)
{
    PickPhysicalDevice(instance, surface);
    CreateLogicalDevice(surface);
    CreateCommandPool();
    CreateAllocator(instance);

    // Transient one-time command buffer + fence for staging uploads.
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = CommandPool;
    allocInfo.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(Device, &allocInfo, &m_ImmediateCmd));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(Device, &fenceInfo, nullptr, &m_ImmediateFence));

    DMGE_LOG_INFO("Vulkan logical device created (GPU: {0})", Properties.deviceName);
}

// ── Physical device selection ──────────────────────────────────────

VulkanDevice::QueueFamilyIndices
VulkanDevice::FindQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) const
{
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.Graphics = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
        if (presentSupport)
            indices.Present = i;

        if (indices.IsComplete())
            break;
    }
    return indices;
}

bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice dev) const
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> avail(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, avail.data());

    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& ext : avail)
        required.erase(ext.extensionName);
    return required.empty();
}

bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice dev, VkSurfaceKHR surface) const
{
    auto families = FindQueueFamilies(dev, surface);
    bool extensionsOk = CheckDeviceExtensionSupport(dev);

    bool swapchainAdequate = false;
    if (extensionsOk)
    {
        uint32_t formatCount = 0, modeCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, nullptr);
        swapchainAdequate = (formatCount > 0 && modeCount > 0);
    }
    return families.IsComplete() && extensionsOk && swapchainAdequate;
}

void VulkanDevice::PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    DMGE_CORE_ASSERT(count > 0, "Failed to find any Vulkan-capable GPU!");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    // Prefer discrete GPUs, then fall back to any suitable device.
    VkPhysicalDevice best = VK_NULL_HANDLE;
    int bestScore = -1;
    for (auto dev : devices)
    {
        if (!IsDeviceSuitable(dev, surface))
            continue;

        vkGetPhysicalDeviceProperties(dev, &Properties);
        int score = 0;
        switch (Properties.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score = 4; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 3; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    score = 2; break;
            default:                                      score = 1; break;
        }
        if (score > bestScore)
        {
            bestScore = score;
            best = dev;
            DeviceType = Properties.deviceType;
        }
    }

    DMGE_CORE_ASSERT(best != VK_NULL_HANDLE, "Failed to find a suitable Vulkan GPU!");
    PhysicalDevice = best;

    vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

    // Highest sample count common to color + depth.
    VkSampleCountFlags counts = Properties.limits.framebufferColorSampleCounts
                              & Properties.limits.framebufferDepthSampleCounts;
    static const VkSampleCountFlags kPriority[] = {
        VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_16_BIT,
        VK_SAMPLE_COUNT_8_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_2_BIT,
    };
    MaxUsableSampleCount = VK_SAMPLE_COUNT_1_BIT;
    for (auto c : kPriority)
        if (counts & c) { MaxUsableSampleCount = c; break; }

    auto families = FindQueueFamilies(PhysicalDevice, surface);
    GraphicsFamily = families.Graphics;
    PresentFamily  = families.Present;
}

// ── Logical device ────────────────────────────────────────────────

void VulkanDevice::CreateLogicalDevice(VkSurfaceKHR surface)
{
    auto families = FindQueueFamilies(PhysicalDevice, surface);

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueFamilies = { families.Graphics, families.Present };
    float priority = 1.0f;
    for (uint32_t fam : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    // Query supported features; enable sampler anisotropy if available.
    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(PhysicalDevice, &supported);
    VkPhysicalDeviceFeatures enabled{};
    enabled.samplerAnisotropy = supported.samplerAnisotropy;
    enabled.fillModeNonSolid  = supported.fillModeNonSolid;

    // Vulkan 1.3 feature chain: dynamic rendering + synchronization2.
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynRender{};
    dynRender.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynRender.pNext             = nullptr;
    dynRender.dynamicRendering  = VK_TRUE;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2{};
    sync2.sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    sync2.pNext               = &dynRender;
    sync2.synchronization2   = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext                   = &sync2;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();
    createInfo.pEnabledFeatures        = &enabled;

    VK_CHECK(vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device));

    vkGetDeviceQueue(Device, GraphicsFamily, 0, &GraphicsQueue);
    vkGetDeviceQueue(Device, PresentFamily, 0, &PresentQueue);

    DynamicRenderingEnabled  = true;
    Synchronization2Enabled  = true;
    MinUniformBufferOffsetAlignment =
        Properties.limits.minUniformBufferOffsetAlignment;
}

// ── Command pool ──────────────────────────────────────────────────

void VulkanDevice::CreateCommandPool()
{
    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = GraphicsFamily;
    VK_CHECK(vkCreateCommandPool(Device, &info, nullptr, &CommandPool));
}

// ── VMA allocator ─────────────────────────────────────────────────

void VulkanDevice::CreateAllocator(VkInstance instance)
{
    VmaAllocatorCreateInfo info{};
    info.instance        = instance;
    info.physicalDevice  = PhysicalDevice;
    info.device          = Device;
    info.vulkanApiVersion = VK_API_VERSION_1_3;
    VK_CHECK(vmaCreateAllocator(&info, &Allocator));
}

// ── Memory type lookup ────────────────────────────────────────────

uint32_t VulkanDevice::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const
{
    for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) &&
            (MemoryProperties.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    DMGE_CORE_ASSERT(false, "Failed to find suitable Vulkan memory type!");
    return 0;
}

// ── One-time submit helper ─────────────────────────────────────────

void VulkanDevice::ImmediateSubmit(const std::function<void(VkCommandBuffer)>& fn)
{
    vkResetCommandBuffer(m_ImmediateCmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(m_ImmediateCmd, &beginInfo));

    fn(m_ImmediateCmd);

    VK_CHECK(vkEndCommandBuffer(m_ImmediateCmd));

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers     = &m_ImmediateCmd;

    vkResetFences(Device, 1, &m_ImmediateFence);
    VK_CHECK(vkQueueSubmit(GraphicsQueue, 1, &submitInfo, m_ImmediateFence));
    VK_CHECK(vkQueueWaitIdle(GraphicsQueue));
}

// ── Deferred destruction (review item B) ──────────────────────────

void VulkanDevice::PushDeferDestroy(uint32_t bucket, std::function<void()>&& fn)
{
    std::lock_guard<std::mutex> lock(DeletionMutex());
    DeletionBucket(bucket).push_back(std::move(fn));
}

void VulkanDevice::FlushDeletions(uint32_t bucket)
{
    std::vector<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> lock(DeletionMutex());
        batch.swap(DeletionBucket(bucket));
    }

    // Execute outside the lock: closures may log or (indirectly) defer
    // further destroys.
    for (auto& fn : batch)
        fn();
}

void VulkanDevice::FlushAllDeletions()
{
    for (uint32_t i = 0; i < Detail::kDeletionBucketCount; ++i)
        FlushDeletions(i);
}

void VulkanDevice::DeferDestroyTexture(VkSampler sampler, VkImageView view,
                                       VkImage image, VmaAllocation alloc)
{
    if (sampler == VK_NULL_HANDLE && view == VK_NULL_HANDLE && image == VK_NULL_HANDLE)
        return;
    if (!IsInitialized())
        return; // device already shut down: handles are invalid either way

    Get().PushDeferDestroy(VulkanGraphicsContext::CurrentDeletionBucket(),
                     [sampler, view, image, alloc]
    {
        auto& dev = Get();
        if (sampler != VK_NULL_HANDLE) vkDestroySampler(dev.Device, sampler, nullptr);
        if (view != VK_NULL_HANDLE)    vkDestroyImageView(dev.Device, view, nullptr);
        if (image != VK_NULL_HANDLE)   vmaDestroyImage(dev.Allocator, image, alloc);
    });
}

void VulkanDevice::DeferDestroyBuffer(VkBuffer buffer, VmaAllocation alloc)
{
    if (buffer == VK_NULL_HANDLE)
        return;
    if (!IsInitialized())
        return; // device already shut down: handles are invalid either way

    Get().PushDeferDestroy(VulkanGraphicsContext::CurrentDeletionBucket(),
                     [buffer, alloc]
    {
        auto& dev = Get();
        vmaDestroyBuffer(dev.Allocator, buffer, alloc);
    });
}

} // namespace DMGameEngine
