#include "DMGameEngine/Core/Application.h"

#include <chrono>

namespace DMGameEngine {

Application::Application()  = default;
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

// ── Lifecycle hooks (default empty) ──────────────────────────────

void Application::OnInitialize() {}
void Application::OnUpdate(float /*deltaTime*/) {}
void Application::OnRender() {}
void Application::OnShutdown() {}

// ── Private ──────────────────────────────────────────────────────

void Application::Initialize() {
    OnInitialize();
    m_isRunning = true;
}

void Application::MainLoop() {
    using Clock = std::chrono::high_resolution_clock;
    auto previousTime = Clock::now();

    while (m_isRunning) {
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
}

} // namespace DMGameEngine
