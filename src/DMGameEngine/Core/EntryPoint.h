/*
 * DMGameEngine - Entry Point
 *
 * Provides the engine's main() function. Game projects define
 * CreateApplication() and include this header in their main
 * translation unit — no need to write main() themselves.
 *
 * Usage in game project:
 *
 *   #include <DMGameEngine/DMGameEngine.h>
 *   #include <DMGameEngine/Core/EntryPoint.h>
 *
 *   class MyGame : public DMGameEngine::Application { ... };
 *
 *   DMGameEngine::Application* DMGameEngine::CreateApplication() {
 *       return new MyGame();
 *   }
 */

#pragma once

#include "DMGameEngine/Core/Application.h"

namespace DMGameEngine {

// ── Must be defined by the game project ──────────────────────────
extern Application* CreateApplication();

} // namespace DMGameEngine

// ── Engine-provided main() ───────────────────────────────────────
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    auto* app = DMGameEngine::CreateApplication();
    int result = app->Run();
    delete app;
    return result;
}
