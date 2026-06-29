/*
 * DMGameEngine - Input Abstraction
 *
 * Platform-agnostic input interface. Use Input::Get() to access
 * the platform-specific singleton for polling keyboard and mouse state.
 *
 * Two polling modes:
 *   - Continuous: IsKeyPressed() returns true every frame while held.
 *   - Edge:       IsKeyJustPressed() returns true only on the first frame
 *                  the key transitions from released → pressed.
 *
 * BeginFrame() must be called once per frame (before any input queries)
 * to snapshot the previous frame's state for edge detection.
 *
 * Usage:
 *   // Held — continuous movement
 *   if (Input::Get().IsKeyPressed(KeyCode::W))  camera.MoveForward(dt);
 *
 *   // Rising edge — single-shot actions
 *   if (Input::Get().IsKeyJustPressed(KeyCode::Space))  player.Jump();
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/KeyCodes.h"
#include "DMGameEngine/Core/MouseCodes.h"

namespace DMGameEngine {

class DMGE_API Input {
public:
    virtual ~Input() = default;

    // ── Per-frame snapshot ─────────────────────────────────────
    // Must be called once per frame, before any input queries,
    // to capture the previous frame's state for edge detection.
    virtual void BeginFrame() = 0;

    // ── Key state — continuous (true every frame while held) ───
    virtual bool IsKeyPressed(KeyCode keycode) const = 0;

    // ── Key state — rising edge (true only on first press frame)
    virtual bool IsKeyJustPressed(KeyCode keycode) const = 0;

    // ── Mouse button state — continuous ────────────────────────
    virtual bool IsMouseButtonPressed(MouseCode button) const = 0;

    // ── Mouse button state — rising edge ───────────────────────
    virtual bool IsMouseButtonJustPressed(MouseCode button) const = 0;

    // ── Mouse position ─────────────────────────────────────────
    virtual float GetMouseX() const = 0;
    virtual float GetMouseY() const = 0;

    // ── Internal: update state from an event ───────────────────
    virtual void OnEvent(Event& e) = 0;

    // ── Singleton accessor ─────────────────────────────────────
    static Input& Get();
};

} // namespace DMGameEngine
