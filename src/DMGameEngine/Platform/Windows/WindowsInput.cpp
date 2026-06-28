#include "DMGameEngine/Platform/Windows/WindowsInput.h"

#include "DMGameEngine/Core/Application.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace DMGameEngine {

// ═══════════════════════════════════════════════════════════════════
//  Static Input::Get() — function-local static, thread-safe (C++11)
// ═══════════════════════════════════════════════════════════════════

Input& Input::Get() {
    static WindowsInput s_instance;
    return s_instance;
}

// ═══════════════════════════════════════════════════════════════════
//  GLFW window access (lazy-init + cache)
// ═══════════════════════════════════════════════════════════════════

GLFWwindow* WindowsInput::GetGLFWWindow() const {
    if (!m_window) {
        m_window = static_cast<GLFWwindow*>(
            Application::Get().GetWindow().GetNativeWindow());
    }
    return m_window;
}

// ═══════════════════════════════════════════════════════════════════
//  Input state — polled directly from GLFW each call
// ═══════════════════════════════════════════════════════════════════

bool WindowsInput::IsKeyPressed(KeyCode keycode) const {
    auto* window = GetGLFWWindow();
    return window && glfwGetKey(window, static_cast<int>(keycode)) == GLFW_PRESS;
}

bool WindowsInput::IsMouseButtonPressed(MouseCode button) const {
    auto* window = GetGLFWWindow();
    return window && glfwGetMouseButton(window, static_cast<int>(button)) == GLFW_PRESS;
}

float WindowsInput::GetMouseX() const {
    auto* window = GetGLFWWindow();
    if (!window) return 0.0f;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    return static_cast<float>(x);
}

float WindowsInput::GetMouseY() const {
    auto* window = GetGLFWWindow();
    if (!window) return 0.0f;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    return static_cast<float>(y);
}

// ═══════════════════════════════════════════════════════════════════
//  OnEvent — reserved for future non-polling input (e.g. text)
// ═══════════════════════════════════════════════════════════════════

void WindowsInput::OnEvent(Event& /*e*/) {
    // GLFW polling functions (glfwGetKey / glfwGetMouseButton /
    // glfwGetCursorPos) provide the authoritative input state directly.
    // Event-driven tracking is not needed here — this hook is reserved
    // for future platform-specific non-polling input (e.g., IME text).
}

} // namespace DMGameEngine
