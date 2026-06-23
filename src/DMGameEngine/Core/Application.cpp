#include "DMGameEngine/Core/Application.h"

#include "DMGameEngine/Core/Events/ApplicationEvent.h"
#include "DMGameEngine/Core/Log.h"

#include <chrono>

namespace DMGameEngine {

// ── Constructors / Destructor ─────────────────────────────────────

Application::Application()
    : m_windowProps(WindowProps()) {}

Application::Application(const WindowProps& windowProps)
    : m_windowProps(windowProps) {}

Application::~Application() = default;

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

// ── Lifecycle hooks (default empty) ──────────────────────────────

void Application::OnInitialize() {}
void Application::OnUpdate(float /*deltaTime*/) {}
void Application::OnRender() {}
void Application::OnShutdown() {}

// ── Private ──────────────────────────────────────────────────────

void Application::Initialize() {
    // Create the platform window
    m_window = Window::Create(m_windowProps);

    // Default event callback: close → quit
    m_window->SetEventCallback([this](Event& e) {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&) {
            Quit();
            return true;
        });
    });

    OnInitialize();
    m_isRunning = true;
}

void Application::MainLoop() {
    using Clock = std::chrono::high_resolution_clock;
    auto previousTime = Clock::now();

    while (m_isRunning) {
        // Poll window events (input, resize, close, etc.)
        m_window->OnUpdate();

        const auto currentTime  = Clock::now();
        const auto elapsed      = std::chrono::duration<float>(currentTime - previousTime);
        const float deltaTime   = elapsed.count();
        previousTime = currentTime;

        OnUpdate(deltaTime);
        OnRender();
    }
}

void Application::Shutdown() {
    m_isRunning = false;
    OnShutdown();
    m_window.reset();
}

} // namespace DMGameEngine
