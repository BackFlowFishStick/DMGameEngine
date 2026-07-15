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
 *       game.PushLayer(DM::CreateScope<GameLayer>());
 *       game.PushOverlay(DM::CreateScope<DebugOverlay>());
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
class CameraController;  // forward declaration - active camera controller

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

    // ── Active camera controller ───────────────────────────────
    // If set, the controller owns its camera and advances it each frame
    // (OnUpdate / OnEvent); the camera view-projection is fed to
    // Renderer::BeginScene, otherwise the scene begins with identity.
    void SetActiveCameraController(const DM::Ref<CameraController>& controller);
    CameraController* GetActiveCameraController() const;

    // ── Singleton accessor ────────────────────────────────
    static Application& Get();

    // ── Layer management ────────────────────────────────────
    void PushLayer(DM::Scope<Layer> layer);
    void PushOverlay(DM::Scope<Layer> overlay);
    DM::Scope<Layer> PopLayer(Layer* layer);
    DM::Scope<Layer> PopOverlay(Layer* overlay);

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
    DM::Scope<Window>         m_window;
    WindowProps               m_windowProps;
    LayerStack                m_layerStack;
    ImGuiLayer*               m_ImGuiLayer = nullptr;
    DM::Ref<CameraController>  m_ActiveController;
};

} // namespace DMGameEngine

