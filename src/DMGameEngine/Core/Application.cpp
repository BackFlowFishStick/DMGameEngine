#include "DMGameEngine/Core/Application.h"

#include "DMGameEngine/Core/Events/ApplicationEvent.h"
#include "DMGameEngine/Core/Input.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/ImGui/ImGuiLayer.h"

#include <algorithm>
#include <chrono>

namespace DMGameEngine {

// ── Static ──────────────────────────────────────────────────────
Application* Application::s_instance = nullptr;

Application& Application::Get() {
    return *s_instance;
}

// ── Constructors / Destructor ─────────────────────────────────────

Application::Application()
    : m_windowProps(WindowProps()) { s_instance = this; }

Application::Application(const WindowProps& windowProps)
    : m_windowProps(windowProps) { s_instance = this; }

Application::~Application() { s_instance = nullptr; }

// ── Public ───────────────────────────────────────────────────────

int Application::Run() {
    Initialize();

    if (m_isRunning) {
        MainLoop();
    }

    Shutdown();
    return 0;
}

void Application::Quit() {
    m_isRunning = false;
}

bool Application::IsRunning() const {
    return m_isRunning;
}

Window& Application::GetWindow() const {
    return *m_window;
}

Camera* Application::GetActiveCamera() const {
    return m_ActiveCamera.get();
}

void Application::SetActiveCamera(const std::shared_ptr<Camera>& camera) {
    m_ActiveCamera = camera;
}

// ── Layer management ─────────────────────────────────────────────

void Application::PushLayer(std::unique_ptr<Layer> layer) {
    m_layerStack.PushLayer(std::move(layer));
}

void Application::PushOverlay(std::unique_ptr<Layer> overlay) {
    m_layerStack.PushOverlay(std::move(overlay));
}

std::unique_ptr<Layer> Application::PopLayer(Layer* layer) {
    return m_layerStack.PopLayer(layer);
}

std::unique_ptr<Layer> Application::PopOverlay(Layer* overlay) {
    return m_layerStack.PopOverlay(overlay);
}

// ── Lifecycle hooks (default empty) ──────────────────────────────

void Application::OnInitialize() {}
void Application::OnUpdate(Timestep /*ts*/) {}
void Application::OnRender() {}
void Application::OnShutdown() {}

// ── Default event handler ────────────────────────────────────────
void Application::OnEvent(Event& e) {
    // Update the global input state before layers consume events
    Input::Get().OnEvent(e);

    // Keep the render viewport in sync with the window's drawable size.
    // Dispatched before layer propagation so a layer cannot suppress it.
    EventDispatcher viewportDispatcher(e);
    viewportDispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& ev) {
        Renderer::OnWindowResize(static_cast<int>(ev.GetWidth()), static_cast<int>(ev.GetHeight()));
        if (m_ActiveCamera)
            m_ActiveCamera->OnViewportResize(ev.GetWidth(), ev.GetHeight());
        return false;  // do not mark handled; layers may still react
    });

    // Propagate event through layers in reverse order:
    // overlays (UI / tool) consume input before gameplay layers.
    for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
        (*it)->OnEvent(e);
        if (e.Handled)
            return;
    }

    // App-level fallback: close window → quit
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&) {
        Quit();
        return true;
    });
}

// ── Private ──────────────────────────────────────────────────────

void Application::Initialize() {
    // Create the platform window
    m_window = Window::Create(m_windowProps);

    // Bind application-level event handling
    m_window->SetEventCallback([this](Event& e) {
        OnEvent(e);
    });

    Renderer::Init();

    // Sync the render viewport with the initial window size.
    Renderer::OnWindowResize(static_cast<int>(m_window->GetWidth()), static_cast<int>(m_window->GetHeight()));

    // Attach the ImGui overlay - provides the context / frame for the
    // OnImGuiRender() pass used by editor and debug UI layers.
    auto imguiLayer = std::make_unique<ImGuiLayer>();
    m_ImGuiLayer = imguiLayer.get();
    PushOverlay(std::move(imguiLayer));

    OnInitialize();
    m_isRunning = true;
}

void Application::MainLoop() {
    using Clock = std::chrono::high_resolution_clock;
    auto previousTime = Clock::now();

    while (m_isRunning) {
        // ── Input snapshot — capture previous frame state for edge detection
        Input::Get().BeginFrame();

        // ═══════════════════════════════════════════════════════════
        //  Stage 1 — Event Pump
        //
        //  Pull all pending OS events (input, window) and dispatch
        //  them through the callback chain:
        //    GLFW → Application::OnEvent(e) → LayerStack (reverse)
        //
        //  Must run before any layer logic so the current frame
        //  sees the latest input state without a 1-frame delay.
        // ═══════════════════════════════════════════════════════════
        m_window->PollEvents();

        const auto currentTime  = Clock::now();
        const auto elapsed      = std::chrono::duration<float>(currentTime - previousTime);
        previousTime = currentTime;

        // Clamp the frame delta so a stalled frame (window drag, system
        // suspend, or debugger pause) does not produce an oversized
        // timestep that destabilizes motion / physics integrators.
        constexpr float kMaxDeltaTime = 0.1f;
        const float deltaTime = std::clamp(elapsed.count(), 0.0f, kMaxDeltaTime);
        const Timestep ts(deltaTime);

        // ═══════════════════════════════════════════════════════════
        //  Stage 2 — Update
        //
        //  Forward iteration: Platform → Core → Resource → Feature → Tool.
        //  Each layer advances its own simulation / logic tick.
        // ═══════════════════════════════════════════════════════════
        for (auto& layer : m_layerStack)
            layer->OnUpdate(ts);

        OnUpdate(ts);  // fallback when no layers are pushed

        // ═══════════════════════════════════════════════════════════
        //  Stage 3 — Render
        //
        //  Renderer::BeginScene() clears the framebuffer (color +
        //  depth). With an active camera set, its view-projection is
        //  cached for the layers' Submit() calls; otherwise the scene
        //  begins with an identity view-projection. Layers then submit
        //  draw commands before Renderer::EndScene() finalizes the
        //  ImGui pass.
        // ═══════════════════════════════════════════════════════════
        if (m_ActiveCamera)
            Renderer::BeginScene(*m_ActiveCamera);
        else
            Renderer::BeginScene();

        for (auto& layer : m_layerStack)
            layer->OnRender();

        OnRender();  // fallback when no layers are pushed

        Renderer::EndScene();

        // ═══════════════════════════════════════════════════════════
        //  Stage 4 — ImGui
        //
        //  Forward iteration. UI overlays construct their panels.
        //  Called after Render so ImGui draw-data is ready for the
        //  platform backend to present.
        // ═══════════════════════════════════════════════════════════
        m_ImGuiLayer->Begin();
        for (auto& layer : m_layerStack)
            layer->OnImGuiRender();
        m_ImGuiLayer->End();

        // ═══════════════════════════════════════════════════════════
        //  Stage 5 — Swap
        //
        //  Present the rendered frame to the display. The platform
        //  backend performs the buffer swap (e.g. glfwSwapBuffers).
        // ═══════════════════════════════════════════════════════════
        m_window->SwapBuffers();
    }
}

void Application::Shutdown() {
    m_isRunning = false;
    OnShutdown();
    Renderer::Shutdown();
    // Detach every layer (OnDetach) while the GL context is still
    // alive, before the window is torn down. Clear() runs the reverse
    // OnDetach() pass and releases ownership in one step; the previous
    // move-assignment bypassed the destructor and skipped OnDetach().
    m_layerStack.Clear();
    m_ImGuiLayer = nullptr;
    m_window.reset();
}

} // namespace DMGameEngine
