/*
 * DMGameEngine - Application Base Class
 *
 * Game projects should inherit from Application and override
 * the lifecycle hooks to build their game logic.
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

namespace DMGameEngine {

class DMGE_API Application {
public:
    Application();
    virtual ~Application();

    // ── Public: entry point called by game's main() ─────────
    int Run();

    // ── Quit control ────────────────────────────────────────
    void Quit();
    bool IsRunning() const;

protected:
    // ── Lifecycle hooks — override in derived class ─────────
    virtual void OnInitialize();
    virtual void OnUpdate(float deltaTime);
    virtual void OnRender();
    virtual void OnShutdown();

private:
    void Initialize();
    void MainLoop();
    void Shutdown();

    bool m_isRunning = false;
};

} // namespace DMGameEngine
