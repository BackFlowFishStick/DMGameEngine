/*
 * DMGameEngine - Vulkan Graphics Context Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"
#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"

#include "DMGameEngine/Core/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vector>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace DMGameEngine {

VulkanGraphicsContext* VulkanGraphicsContext::s_Instance = nullptr;

namespace {

// Helper: insert an image layout transition barrier (sync1 API).
void TransitionImageLayoutImpl(VkCommandBuffer cmd, VkImage image,
                           VkFormat /*format*/,
                           VkImageLayout oldLayout, VkImageLayout newLayout,
                           VkImageAspectFlags aspect,
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
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer  = 0;
    barrier.subresourceRange.layerCount      = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd,
                         srcStage, dstStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}

VkResult CreateDebugUtilsMessenger(VkInstance instance,
                                   const VkDebugUtilsMessengerCreateInfoEXT* info,
                                   VkDebugUtilsMessengerEXT* messenger)
{
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    return fn ? fn(instance, info, nullptr, messenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn) fn(instance, messenger, nullptr);
}

} // anonymous namespace

// ── Constructor / Destructor ───────────────────────────────────────

VulkanGraphicsContext::VulkanGraphicsContext(GLFWwindow* windowHandle)
    : m_WindowHandle(windowHandle)
{
}

VulkanGraphicsContext::~VulkanGraphicsContext()
{
    if (s_Instance == this) s_Instance = nullptr;
    DestroyFrameResources();

    m_Swapchain.Cleanup();
    VulkanDevice::Shutdown();

    if (m_Surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    if (m_DebugMessenger != VK_NULL_HANDLE)
        DestroyDebugUtilsMessenger(m_Instance, m_DebugMessenger);
    if (m_Instance != VK_NULL_HANDLE)
        vkDestroyInstance(m_Instance, nullptr);
}

// ── Init ─────────────────────────────────────────────────────────────

void VulkanGraphicsContext::Init()
{
    s_Instance = this;
    CreateInstance();
    CreateSurface();
    VulkanDevice::Init(m_Instance, m_Surface);
    if (m_ValidationEnabled)
        SetupDebugMessenger();

    int width = 0, height = 0;
    glfwGetFramebufferSize(m_WindowHandle, &width, &height);
    m_Swapchain.Init(m_Instance, m_Surface,
                     static_cast<uint32_t>(width),
                     static_cast<uint32_t>(height));

    CreateFrameResources();

    DMGE_LOG_INFO("Vulkan Info:");
    DMGE_LOG_INFO("  Vendor:   {0}", GetVendor());
    DMGE_LOG_INFO("  Renderer: {0}", GetRenderer());
    DMGE_LOG_INFO("  Version:  {0}", GetVersion());
}

void VulkanGraphicsContext::CreateInstance()
{
#ifdef DMGE_ENABLE_ASSERTS
    m_ValidationEnabled = true;
#endif

    if (m_ValidationEnabled)
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
        bool found = false;
        for (const auto& l : layers)
            if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0)
                { found = true; break; }
        if (!found)
        {
            DMGE_LOG_WARN("Validation layer requested but unavailable; running without it.");
            m_ValidationEnabled = false;
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName    = "DMGameEngine";
    appInfo.applicationVersion  = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName         = "DMGameEngine";
    appInfo.engineVersion       = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion          = VK_API_VERSION_1_3;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);
    if (m_ValidationEnabled)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo         = &appInfo;
    info.enabledExtensionCount    = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames  = extensions.data();

    std::vector<const char*> layers;
    if (m_ValidationEnabled)
        layers.push_back("VK_LAYER_KHRONOS_validation");
    info.enabledLayerCount   = static_cast<uint32_t>(layers.size());
    info.ppEnabledLayerNames = layers.data();

    VK_CHECK(vkCreateInstance(&info, nullptr, &m_Instance));
}

void VulkanGraphicsContext::CreateSurface()
{
    VK_CHECK(glfwCreateWindowSurface(m_Instance, m_WindowHandle, nullptr, &m_Surface));
}

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanGraphicsContext::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                      VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                                      const VkDebugUtilsMessengerCallbackDataEXT* data,
                                      void* /*userData*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        DMGE_LOG_ERROR("[Vulkan Validation] {0}", data->pMessage);
    return VK_FALSE;
}

void VulkanGraphicsContext::SetupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;
    CreateDebugUtilsMessenger(m_Instance, &info, &m_DebugMessenger);
}

void VulkanGraphicsContext::CreateFrameResources()
{
    auto& dev = VulkanDevice::Get();

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = dev.GraphicsFamily;
        VK_CHECK(vkCreateCommandPool(dev.Device, &poolInfo, nullptr, &m_CommandPools[i]));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool         = m_CommandPools[i];
        allocInfo.level               = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount  = 1;
        VK_CHECK(vkAllocateCommandBuffers(dev.Device, &allocInfo, &m_CommandBuffers[i]));

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(dev.Device, &semInfo, nullptr, &m_ImageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(dev.Device, &semInfo, nullptr, &m_RenderFinishedSemaphores[i]));

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // first wait succeeds
        VK_CHECK(vkCreateFence(dev.Device, &fenceInfo, nullptr, &m_InFlightFences[i]));
    }
}

void VulkanGraphicsContext::DestroyFrameResources()
{
    auto& dev = VulkanDevice::Get();
    if (dev.Device == VK_NULL_HANDLE)
        return;

    vkDeviceWaitIdle(dev.Device);

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(dev.Device, m_ImageAvailableSemaphores[i], nullptr);
        if (m_RenderFinishedSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(dev.Device, m_RenderFinishedSemaphores[i], nullptr);
        if (m_InFlightFences[i] != VK_NULL_HANDLE)
            vkDestroyFence(dev.Device, m_InFlightFences[i], nullptr);
        if (m_CommandPools[i] != VK_NULL_HANDLE)
            vkDestroyCommandPool(dev.Device, m_CommandPools[i], nullptr);
    }
}

void VulkanGraphicsContext::RecreateSwapchain()
{
    // Always query the real framebuffer pixel size. RequestResize only sets
    // the m_NeedsResize flag (the w/h it receives are window-space / logical
    // units and may differ from framebuffer pixels under DPI scaling), so
    // glfwGetFramebufferSize is the correct source for the swapchain extent.
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_WindowHandle, &width, &height);

    if (width == 0 || height == 0)
        return; // minimized window: defer until non-zero

    // The old swapchain images and the cached pipelines (whose keys bake the
    // old formats) may still be referenced by an in-flight submission.
    // Recreate is a rare, resize-driven event: idle the device first so the
    // destroys below are not use-while-in-flight (review item A).
    auto& dev = VulkanDevice::Get();
    if (dev.Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(dev.Device);

    m_Swapchain.Recreate(static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height));

    // Invalidate cached pipelines: their PipelineKey bakes the swapchain
    // color/depth format; if the new swapchain has a different format the
    // cached pipelines would mismatch (VUID-vkCmdBeginRendering-None-06197).
    if (auto* rapi = VulkanRendererAPI::Get())
        rapi->OnSwapchainRecreate();
}

// ── Frame lifecycle ────────────────────────────────────────────────

void VulkanGraphicsContext::TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                                  VkFormat format,
                                                  VkImageLayout oldLayout, VkImageLayout newLayout,
                                                  VkImageAspectFlags aspect,
                                                  VkPipelineStageFlags srcStage, VkAccessFlags srcAccess,
                                                  VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    // Forwards to the file-local sync1 barrier helper (anonymous namespace).
    TransitionImageLayoutImpl(cmd, image, format, oldLayout, newLayout, aspect,
                              srcStage, srcAccess, dstStage, dstAccess);
}

void VulkanGraphicsContext::BeginFrame(const glm::vec4& clearColor)
{
    auto& dev = VulkanDevice::Get();

    if (m_NeedsResize)
    {
        RecreateSwapchain();
        m_NeedsResize = false;
    }

    // Wait for the previous frame using this slot to finish.
    VK_CHECK(vkWaitForFences(dev.Device, 1, &m_InFlightFences[m_CurrentFrame],
                             VK_TRUE, UINT64_MAX));

    VkResult result = m_Swapchain.AcquireNextImage(m_ImageAvailableSemaphores[m_CurrentFrame],
                                                    &m_ImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain();
        m_FrameStarted = false;
        return; // skip this frame; will retry next BeginFrame
    }
    if (result == VK_SUBOPTIMAL_KHR)
        m_NeedsResize = true; // recreate next frame
    else if (result != VK_SUCCESS)
    {
        DMGE_LOG_ERROR("Failed to acquire swapchain image ({0})", static_cast<int>(result));
        m_FrameStarted = false;
        return;
    }

    // Begin recording.
    VK_CHECK(vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0));
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo));

    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    // Transition the acquired color image UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL.
    TransitionImageLayoutImpl(cmd, m_Swapchain.GetImage(m_ImageIndex),
                          m_Swapchain.GetImageFormat(),
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    // Transition depth image UNDEFINED -> DEPTH_ATTACHMENT_OPTIMAL.
    TransitionImageLayoutImpl(cmd, m_Swapchain.GetDepthImage(), m_Swapchain.GetDepthFormat(),
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_ASPECT_DEPTH_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    // Begin dynamic rendering (clears color + depth).
    VkClearValue colorClear{};
    colorClear.color.float32[0] = clearColor.r;
    colorClear.color.float32[1] = clearColor.g;
    colorClear.color.float32[2] = clearColor.b;
    colorClear.color.float32[3] = clearColor.a;

    VkClearValue depthClear{};
    depthClear.depthStencil.depth   = 1.0f;
    depthClear.depthStencil.stencil  = 0;

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType          = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView       = m_Swapchain.GetImageView(m_ImageIndex);
    colorAttachment.imageLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue       = colorClear;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType          = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView       = m_Swapchain.GetDepthView();
    depthAttachment.imageLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue       = depthClear;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset    = { 0, 0 };
    renderingInfo.renderArea.extent     = m_Swapchain.GetExtent();
    renderingInfo.layerCount            = 1;
    renderingInfo.colorAttachmentCount  = 1;
    renderingInfo.pColorAttachments     = &colorAttachment;
    renderingInfo.pDepthAttachment      = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    // Default viewport+scissor = full swapchain extent.
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_Swapchain.GetExtent().width);
    viewport.height   = static_cast<float>(m_Swapchain.GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_Swapchain.GetExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_FrameStarted = true;
}

void VulkanGraphicsContext::EndFrame()
{
    if (!m_FrameStarted)
        return; // frame was skipped (swapchain out-of-date / minimized)

    auto& dev = VulkanDevice::Get();
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    vkCmdEndRendering(cmd);

    // Transition color image COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR.
    TransitionImageLayoutImpl(cmd, m_Swapchain.GetImage(m_ImageIndex),
                          m_Swapchain.GetImageFormat(),
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_ImageAvailableSemaphores[m_CurrentFrame];
    submitInfo.pWaitDstStageMask     = &waitStage;
    submitInfo.commandBufferCount     = 1;
    submitInfo.pCommandBuffers       = &cmd;
    submitInfo.signalSemaphoreCount  = 1;
    submitInfo.pSignalSemaphores     = &m_RenderFinishedSemaphores[m_CurrentFrame];

    VK_CHECK(vkResetFences(dev.Device, 1, &m_InFlightFences[m_CurrentFrame]));
    VK_CHECK(vkQueueSubmit(dev.GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]));

    VkResult result = m_Swapchain.Present(m_RenderFinishedSemaphores[m_CurrentFrame], m_ImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        m_NeedsResize = true;

    m_CurrentFrame = (m_CurrentFrame + 1) % kMaxFramesInFlight;
    m_FrameStarted = false;
}

void VulkanGraphicsContext::SwapBuffers()
{
    EndFrame();
}

const char* VulkanGraphicsContext::GetVendor() const
{
    static std::string vendor;
    vendor = VulkanDevice::Get().Properties.deviceName;
    // Properties has no separate vendor string; report the driver name.
    return vendor.c_str();
}

const char* VulkanGraphicsContext::GetRenderer() const
{
    switch (VulkanDevice::Get().DeviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
        default:                                       return "Unknown GPU";
    }
}

const char* VulkanGraphicsContext::GetVersion() const
{
    static char version[64];
    const auto& p = VulkanDevice::Get().Properties;
    std::snprintf(version, sizeof(version), "Vulkan %u.%u.%u (driver %u.%u.%u)",
                  VK_API_VERSION_MAJOR(p.apiVersion),
                  VK_API_VERSION_MINOR(p.apiVersion),
                  VK_API_VERSION_PATCH(p.apiVersion),
                  VK_API_VERSION_MAJOR(p.driverVersion),
                  VK_API_VERSION_MINOR(p.driverVersion),
                  VK_API_VERSION_PATCH(p.driverVersion));
    return version;
}

} // namespace DMGameEngine
