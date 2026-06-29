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

    mutable GLFWwindow* m_window = nullptr;
};

} // namespace DMGameEngine
