/*
 * DMGameEngine - Application Base Class
 *
 * Game projects should inherit from Application and override
 * the lifecycle hooks to build their game logic.
 *
 * A default GLFW window is created automatically during initialization.
 * Access it via GetWindow() to configure callbacks, VSync, etc.
 *
 * Layers can be pushed onto the built-in LayerStack. The Application
 * automatically propagates OnUpdate, OnRender, OnImGuiRender, and OnEvent
 * to all layers each frame.
 *
 * Usage:
 *   class MyGame : public DMGameEngine::Application {
 *   public:
 *       void OnInitialize() override { ... }
 *       void OnUpdate(Timestep ts) override { ... }
 *       void OnRender() override      { ... }
 *       void OnShutdown() override    { ... }
 *   };
 *
 *   int main() {
 *       MyGame game;
 *       game.PushLayer(std::make_unique<GameLayer>());
 *       game.PushOverlay(std::make_unique<DebugOverlay>());
 *       return game.Run();
 *   }
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Core/LayerStack.h"
#include "DMGameEngine/Core/Window.h"

#include <memory>

namespace DMGameEngine {

class ImGuiLayer;
class Camera;      // forward declaration - active scene view-projection

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

    // ImGui overlay (attached automatically); null until Initialize()
    ImGuiLayer* GetImGuiLayer() const { return m_ImGuiLayer; }

    // ── Active scene camera ──────────────────────────────────
    // If set, its view-projection is fed to Renderer::BeginScene each
    // frame; otherwise the scene begins with an identity view-projection.
    void SetActiveCamera(const std::shared_ptr<Camera>& camera);
    Camera* GetActiveCamera() const;

    // ── Singleton accessor ────────────────────────────────
    static Application& Get();

    // ── Layer management ────────────────────────────────────
    void PushLayer(std::unique_ptr<Layer> layer);
    void PushOverlay(std::unique_ptr<Layer> overlay);
    std::unique_ptr<Layer> PopLayer(Layer* layer);
    std::unique_ptr<Layer> PopOverlay(Layer* overlay);

protected:
    // ── Lifecycle hooks — override in derived class ─────────
    virtual void OnInitialize();
    virtual void OnUpdate(Timestep ts);
    virtual void OnRender();
    virtual void OnShutdown();

    // ── Event binding — override to handle window events ────
    //     Default implementation handles WindowCloseEvent → Quit().
    //     Events are propagated through the LayerStack (reverse
    //     order) before reaching this handler.
    //     Call Application::OnEvent(e) from your override to keep it.
    virtual void OnEvent(Event& e);

private:
    void Initialize();
    void MainLoop();
    void Shutdown();

    static Application*       s_instance;
    bool                      m_isRunning = false;
    std::unique_ptr<Window>   m_window;
    WindowProps               m_windowProps;
    LayerStack                m_layerStack;
    ImGuiLayer*               m_ImGuiLayer = nullptr;
    std::shared_ptr<Camera>   m_ActiveCamera;
};

} // namespace DMGameEngine
