#include "DMGameEngine/Core/Application.h"

#include "DMGameEngine/Core/Events/ApplicationEvent.h"
#include "DMGameEngine/Core/Input.h"
#include "DMGameEngine/Core/Log.h"

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
void Application::OnUpdate(float /*deltaTime*/) {}
void Application::OnRender() {}
void Application::OnShutdown() {}

// ── Default event handler ────────────────────────────────────────
void Application::OnEvent(Event& e) {
    // Update the global input state before layers consume events
    Input::Get().OnEvent(e);

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

    OnInitialize();
    m_isRunning = true;
}

void Application::MainLoop() {
    using Clock = std::chrono::high_resolution_clock;
    auto previousTime = Clock::now();

    while (m_isRunning) {
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
        const float deltaTime   = elapsed.count();
        previousTime = currentTime;

        // ═══════════════════════════════════════════════════════════
        //  Stage 2 — Update
        //
        //  Forward iteration: Platform → Core → Resource → Feature → Tool.
        //  Each layer advances its own simulation / logic tick.
        // ═══════════════════════════════════════════════════════════
        for (auto& layer : m_layerStack)
            layer->OnUpdate(deltaTime);

        OnUpdate(deltaTime);  // fallback when no layers are pushed

        // ═══════════════════════════════════════════════════════════
        //  Stage 3 — Render
        //
        //  Forward iteration. Each layer submits draw commands.
        // ═══════════════════════════════════════════════════════════
        for (auto& layer : m_layerStack)
            layer->OnRender();

        OnRender();  // fallback when no layers are pushed

        // ═══════════════════════════════════════════════════════════
        //  Stage 4 — ImGui
        //
        //  Forward iteration. UI overlays construct their panels.
        //  Called after Render so ImGui draw-data is ready for the
        //  platform backend to present.
        // ═══════════════════════════════════════════════════════════
        for (auto& layer : m_layerStack)
            layer->OnImGuiRender();
    }
}

void Application::Shutdown() {
    m_isRunning = false;
    OnShutdown();
    // LayerStack destructor automatically calls OnDetach()
    // for all layers in reverse order, then clears the stack.
    m_layerStack = LayerStack{};
    m_window.reset();
}

} // namespace DMGameEngine
