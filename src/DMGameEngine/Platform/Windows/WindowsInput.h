/*
 * DMGameEngine - Windows Input (GLFW Polling-based)
 *
 * Concrete Input implementation that polls GLFW directly for
 * keyboard and mouse state via the Application → Window → GLFWwindow chain.
 */

#pragma once

#include "DMGameEngine/Core/Input.h"

struct GLFWwindow;

namespace DMGameEngine {

class WindowsInput : public Input {
public:
    bool IsKeyPressed(KeyCode keycode) const override;
    bool IsMouseButtonPressed(MouseCode button) const override;
    float GetMouseX() const override;
    float GetMouseY() const override;
    void OnEvent(Event& e) override;

private:
    // Lazy-init GLFWwindow* through Application → Window → GetNativeWindow
    GLFWwindow* GetGLFWWindow() const;

    mutable GLFWwindow* m_window = nullptr;
};

} // namespace DMGameEngine
