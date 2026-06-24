/*
 * DMGameEngine - Application Base Class
 *
 * Game projects should inherit from Application and override
 * the lifecycle hooks to build their game logic.
 *
 * A default GLFW window is created automatically during initialization.
 * Access it via GetWindow() to configure callbacks, VSync, etc.
 *
 * Usage:
 *   class MyGame : public DMGameEngine::Application {
 *   public:
 *       void OnInitialize() override { ... }
 *       void OnUpdate(float dt) override { ... }
 *       void OnRender() override      { ... }
 *       void OnShutdown() override    { ... }
 *   };
 *
 *   int main() {
 *       MyGame game;
 *       return game.Run();
 *   }
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Window.h"

#include <memory>

namespace DMGameEngine {

class DMGE_API Application {
public:
    Application();
    explicit Application(const WindowProps& windowProps);
    virtual ~Application();

    // ── Public: entry point called by game's main() ─────────
    int Run();

    // ── Quit control ────────────────────────────────────────
    void Quit();
    bool IsRunning() const;

    // ── Window access ───────────────────────────────────────
    Window& GetWindow() const;

protected:
    // ── Lifecycle hooks — override in derived class ─────────
    virtual void OnInitialize();
    virtual void OnUpdate(float deltaTime);
    virtual void OnRender();
    virtual void OnShutdown();

    // ── Event binding — override to handle window events ────
    //     Default implementation handles WindowCloseEvent → Quit().
    //     Call Application::OnEvent(e) from your override to keep it.
    virtual void OnEvent(Event& e);

private:
    void Initialize();
    void MainLoop();
    void Shutdown();

    bool                      m_isRunning = false;
    std::unique_ptr<Window>   m_window;
    WindowProps               m_windowProps;
};

} // namespace DMGameEngine
