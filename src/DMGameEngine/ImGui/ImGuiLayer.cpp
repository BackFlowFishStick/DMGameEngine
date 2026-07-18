#include "DMGameEngine/ImGui/ImGuiLayer.h"

#include "DMGameEngine/Core/Application.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Core/Window.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#ifdef DMGE_VULKAN
#include <imgui_impl_vulkan.h>
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanSwapchain.h"
#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"
#endif

#include "DMGameEngine/Renderer/Renderer.h"

namespace DMGameEngine {

ImGuiLayer::ImGuiLayer()
    : Layer("ImGuiLayer", LayerType::Tool) {}

#ifdef DMGE_VULKAN
static void ImGuiVkCheckResult(VkResult err)
{
    if (err != VK_SUCCESS)
        DMGE_LOG_ERROR("ImGui Vulkan backend error: VkResult {}", static_cast<int>(err));
}
#endif

// ── Lifecycle ────────────────────────────────────────────────────

void ImGuiLayer::OnAttach() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // The ImGui GLFW backend installs its own GLFW callbacks and chains
    // the engine's existing ones, so both ImGui and the engine receive
    // input. The engine callbacks are already in place because the
    // window is created before any layer attaches.
    GLFWwindow* window = static_cast<GLFWwindow*>(
        Application::Get().GetWindow().GetNativeWindow());

#ifdef DMGE_VULKAN
    if (Renderer::GetAPI() == Renderer::API::Vulkan)
    {
        ImGui_ImplGlfw_InitForVulkan(window, true);

        // Draw into the swapchain color image the context already has open
        // for the frame: VulkanGraphicsContext keeps the dynamic-rendering
        // pass open across EndScene()/ImGui until SwapBuffers().
        auto& ctx  = VulkanGraphicsContext::Get();
        auto& dev  = VulkanDevice::Get();
        auto& swap = ctx.GetSwapchain();
        VkFormat colorFormat = swap.GetImageFormat();

        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion            = VK_API_VERSION_1_3;
        info.Instance              = ctx.GetInstance();
        info.PhysicalDevice        = dev.PhysicalDevice;
        info.Device                = dev.Device;
        info.QueueFamily           = dev.GraphicsFamily;
        info.Queue                 = dev.GraphicsQueue;
        info.DescriptorPoolSize    = 1000;  // backend creates + owns the pool
        info.MinImageCount         = 2;
        info.ImageCount            = swap.GetImageCount();
        info.Allocator             = nullptr;
        info.CheckVkResultFn       = &ImGuiVkCheckResult;
        info.UseDynamicRendering   = true;
        info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineRenderingCreateInfo& prci = info.PipelineInfoMain.PipelineRenderingCreateInfo;
        prci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        prci.colorAttachmentCount    = 1;
        prci.pColorAttachmentFormats  = &colorFormat;

        ImGui_ImplVulkan_Init(&info);
    }
    else
#endif
    {
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(nullptr);  // default GLSL version (#version 130)
    }

    DMGE_LOG_INFO("ImGuiLayer attached (ImGui {})", IMGUI_VERSION);
}

void ImGuiLayer::OnDetach() {
#ifdef DMGE_VULKAN
    if (Renderer::GetAPI() == Renderer::API::Vulkan)
        ImGui_ImplVulkan_Shutdown();
    else
#endif
        ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    DMGE_LOG_INFO("ImGuiLayer detached");
}

// ── Per-frame ImGui pass ─────────────────────────────────────────

void ImGuiLayer::Begin() {
#ifdef DMGE_VULKAN
    if (Renderer::GetAPI() == Renderer::API::Vulkan)
        ImGui_ImplVulkan_NewFrame();
    else
#endif
        ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::End() {
    ImGui::Render();
#ifdef DMGE_VULKAN
    if (Renderer::GetAPI() == Renderer::API::Vulkan)
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                        VulkanGraphicsContext::Get().GetCurrentCommandBuffer());
    else
#endif
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ── Optional demo ────────────────────────────────────────────────

void ImGuiLayer::OnImGuiRender() {
    if (m_ShowDemo)
        ImGui::ShowDemoWindow(&m_ShowDemo);
}

// ── Input capture ────────────────────────────────────────────────

void ImGuiLayer::OnEvent(Event& event) {
    const ImGuiIO& io = ImGui::GetIO();

    // Cursor is over an ImGui window - let ImGui own the mouse.
    if (io.WantCaptureMouse && event.IsInCategory(EventCategory::Mouse)) {
        event.Handled = true;
        return;
    }
    // ImGui owns the keyboard (e.g. a text input is focused).
    if (io.WantCaptureKeyboard && event.IsInCategory(EventCategory::Keyboard)) {
        event.Handled = true;
        return;
    }
}

} // namespace DMGameEngine