/*
 * DMGameEngine - Vulkan Graphics Context
 *
 * Vulkan implementation of the GraphicsContext abstraction. Owns the
 * VkInstance (+ debug messenger), the window VkSurfaceKHR, the
 * VulkanDevice and VulkanSwapchain, and the per-frame command buffers /
 * synchronization that implement the frame lifecycle:
 *
 *   BeginScene()  -> Clear() -> BeginFrame()
 *       acquire swapchain image, begin command buffer, transition the
 *       color/depth images, begin dynamic rendering (clears color+depth)
 *   draws recorded into the current command buffer
 *   SwapBuffers() -> EndFrame()
 *       end dynamic rendering, transition color -> PRESENT_SRC, submit,
 *       present, advance the frame-in-flight index
 *
 * The frame stays open across EndScene()/ImGui so a future Vulkan ImGui
 * backend can record its draws before SwapBuffers().
 */

#pragma once

#include "DMGameEngine/Renderer/GraphicsContext.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanSwapchain.h"
#include "glm/glm.hpp"

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace DMGameEngine {

class DMGE_API VulkanGraphicsContext : public GraphicsContext
{
public:
    explicit VulkanGraphicsContext(GLFWwindow* windowHandle);
    ~VulkanGraphicsContext() override;

    void Init() override;
    void SwapBuffers() override;

    const char* GetVendor()   const override;
    const char* GetRenderer() const override;
    const char* GetVersion()  const override;

    // ── Frame lifecycle (used by VulkanRendererAPI) ─────────────
    void BeginFrame(const glm::vec4& clearColor);
    void EndFrame();

    VkCommandBuffer GetCurrentCommandBuffer() const { return m_CommandBuffers[m_CurrentFrame]; }
    uint32_t        GetCurrentImageIndex()    const { return m_ImageIndex; }
    uint32_t        GetCurrentFrame()         const { return m_CurrentFrame; }
    bool            IsFrameStarted()          const { return m_FrameStarted; }

    VulkanSwapchain& GetSwapchain() { return m_Swapchain; }

    // Inserts an image layout transition barrier (sync1 API). Public so the
    // renderer API can transition offscreen FrameBuffer attachments.
    static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                      VkFormat format,
                                      VkImageLayout oldLayout, VkImageLayout newLayout,
                                      VkImageAspectFlags aspect,
                                      VkPipelineStageFlags srcStage, VkAccessFlags srcAccess,
                                      VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    VkInstance       GetInstance() const { return m_Instance; }

    static VulkanGraphicsContext& Get() { return *s_Instance; }

    // Called when the window is resized so the swapchain is recreated
    // at the next BeginFrame().
    void RequestResize(uint32_t width, uint32_t height)
    {
        m_ResizeWidth  = width;
        m_ResizeHeight = height;
        m_NeedsResize  = true;
    }

private:
    void CreateInstance();
    void CreateSurface();
    void SetupDebugMessenger();
    void CreateFrameResources();
    void DestroyFrameResources();

    void RecreateSwapchain();

    static VulkanGraphicsContext* s_Instance;

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                  VkDebugUtilsMessageTypeFlagsEXT type,
                  const VkDebugUtilsMessengerCallbackDataEXT* data,
                  void* userData);

    GLFWwindow* m_WindowHandle = nullptr;

    VkInstance              m_Instance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger  = VK_NULL_HANDLE;
    VkSurfaceKHR            m_Surface         = VK_NULL_HANDLE;

    VulkanSwapchain m_Swapchain;

    static constexpr uint32_t kMaxFramesInFlight = 2;
    VkCommandPool m_CommandPools[kMaxFramesInFlight]   = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkCommandBuffer m_CommandBuffers[kMaxFramesInFlight] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkSemaphore m_ImageAvailableSemaphores[kMaxFramesInFlight] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkSemaphore m_RenderFinishedSemaphores[kMaxFramesInFlight] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkFence     m_InFlightFences[kMaxFramesInFlight]   = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    uint32_t m_CurrentFrame  = 0;
    uint32_t m_ImageIndex     = 0;
    bool     m_FrameStarted   = false;

    // Deferred swapchain recreation (set by RequestResize or on acquire).
    bool     m_NeedsResize    = false;
    uint32_t m_ResizeWidth     = 0;
    uint32_t m_ResizeHeight     = 0;

    bool m_ValidationEnabled = false;
};

} // namespace DMGameEngine
