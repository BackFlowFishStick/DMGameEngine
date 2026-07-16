/*
 * DMGameEngine - GLFW Input (GLFW Polling + Event Edge Detection)
 *
 * Concrete Input implementation with two query modes:
 *   - Continuous (IsKeyPressed): polls glfwGetKey directly for authoritative
 *     held-or-not state, independent of event consumption by layers.
 *   - Edge (IsKeyJustPressed): combines GLFW current state with a per-frame
 *     snapshot of the previous frame's event-tracked state for rising-edge
 *     detection.
 *
 * BeginFrame() must be called once per frame before any input queries.
 */

#pragma once

#include "DMGameEngine/Core/Input.h"

#include <array>
#include <string>
#include <unordered_map>

struct GLFWwindow;

namespace DMGameEngine {

class GLFWInput : public Input {
public:
    void BeginFrame() override;

    bool IsKeyPressed(KeyCode keycode) const override;
    bool IsKeyJustPressed(KeyCode keycode) const override;
    bool IsMouseButtonPressed(MouseCode button) const override;
    bool IsMouseButtonJustPressed(MouseCode button) const override;

    float GetMouseX() const override;
    float GetMouseY() const override;

    // Mouse motion delta (per frame)
    float GetMouseDeltaX() const override;
    float GetMouseDeltaY() const override;

    // Gamepad (controller) state
    bool        IsGamepadPresent(int index) const override;
    std::string GetGamepadName(int index) const override;
    bool        IsGamepadButtonPressed(int index, GamepadButton button) const override;
    bool        IsGamepadButtonJustPressed(int index, GamepadButton button) const override;
    float       GetGamepadAxis(int index, GamepadAxis axis) const override;

    void OnEvent(Event& e) override;

private:
    GLFWwindow* GetGLFWWindow() const;

    // Current-frame state (updated by OnEvent)
    std::unordered_map<KeyCode, bool>   m_keyState;
    std::unordered_map<MouseCode, bool> m_mouseButtonState;

    // Previous-frame state (snapshotted by BeginFrame)
    std::unordered_map<KeyCode, bool>   m_prevKeyState;
    std::unordered_map<MouseCode, bool> m_prevMouseButtonState;

    // Per-frame "just pressed" flags — set by OnEvent only on repeatCount==0,
    // cleared by BeginFrame. Used by IsKeyJustPressed / IsMouseButtonJustPressed.
    std::unordered_map<KeyCode, bool>   m_keyJustPressed;
    std::unordered_map<MouseCode, bool> m_mouseButtonJustPressed;

    // Mouse motion delta - accumulated by OnEvent (MouseMovedEvent),
    // reset to zero by BeginFrame. m_lastMouseX/Y tracks the running
    // cursor position so consecutive events produce smooth deltas.
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
    float m_mouseDeltaX = 0.0f;
    float m_mouseDeltaY = 0.0f;
    bool  m_mousePosInitialized = false;

    // Gamepad previous-frame button state (for just-pressed edge
    // detection). Gamepads expose no press/release events, so the
    // previous state is polled once per frame in BeginFrame.
    using GamepadButtonRow = std::array<bool, static_cast<std::size_t>(GamepadButton::Count)>;
    std::array<GamepadButtonRow, static_cast<std::size_t>(kMaxGamepads)> m_prevGamepadButtons{};

    mutable GLFWwindow* m_window = nullptr;
};

} // namespace DMGameEngine
