/*
 * DMGameEngine - Input Abstraction
 *
 * Platform-agnostic input interface. Use Input::Get() to access
 * the platform-specific singleton for polling keyboard and mouse state.
 *
 * Usage:
 *   if (Input::Get().IsKeyPressed(KeyCode::W)) { ... }
 *   if (Input::Get().IsMouseButtonPressed(MouseCode::Left)) { ... }
 *   float mouseX = Input::Get().GetMouseX();
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"

namespace DMGameEngine {

class DMGE_API Input {
public:
    virtual ~Input() = default;

    // ── Key state ─────────────────────────────────────────────
    virtual bool IsKeyPressed(KeyCode keycode) const = 0;

    // ── Mouse button state ────────────────────────────────────
    virtual bool IsMouseButtonPressed(MouseCode button) const = 0;

    // ── Mouse position ────────────────────────────────────────
    virtual float GetMouseX() const = 0;
    virtual float GetMouseY() const = 0;

    // ── Internal: update state from an event ──────────────────
    // Called by the engine before layer propagation so layers
    // see the latest input state without a 1-frame delay.
    virtual void OnEvent(Event& e) = 0;

    // ── Singleton accessor ────────────────────────────────────
    // Returns the platform-specific Input instance. Initialised
    // lazily on first access (thread-safe, C++11).
    static Input& Get();
};

} // namespace DMGameEngine
