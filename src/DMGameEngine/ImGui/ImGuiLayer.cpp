#include "DMGameEngine/ImGui/ImGuiLayer.h"

#include "DMGameEngine/Core/Application.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Core/Window.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "DMGameEngine/Renderer/Renderer.h"

namespace DMGameEngine {

ImGuiLayer::ImGuiLayer()
    : Layer("ImGuiLayer", LayerType::Tool) {}

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

    if (Renderer::GetAPI() == Renderer::API::Vulkan)
        ImGui_ImplGlfw_InitForVulkan(window, true);
    else
        ImGui_ImplGlfw_InitForOpenGL(window, true);

    // NOTE: a full Vulkan ImGui render backend (imgui_impl_vulkan) is not
    // wired up here yet; under Vulkan the GL renderer backend is skipped so
    // the engine renders its scene without ImGui draw data. Add a Vulkan
    // ImGui backend to get visible UI panels under Vulkan.
    if (Renderer::GetAPI() != Renderer::API::Vulkan)
        ImGui_ImplOpenGL3_Init(nullptr);  // default GLSL version (#version 130)

    DMGE_LOG_INFO("ImGuiLayer attached (ImGui {})", IMGUI_VERSION);
}

void ImGuiLayer::OnDetach() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    DMGE_LOG_INFO("ImGuiLayer detached");
}

// ── Per-frame ImGui pass ─────────────────────────────────────────

void ImGuiLayer::Begin() {
    if (Renderer::GetAPI() != Renderer::API::Vulkan)
        ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::End() {
    ImGui::Render();
    if (Renderer::GetAPI() != Renderer::API::Vulkan)
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