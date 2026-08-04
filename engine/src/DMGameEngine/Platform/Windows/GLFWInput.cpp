#include "DMGameEngine/Platform/Windows/GLFWInput.h"

#include "DMGameEngine/Core/Application.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace DMGameEngine {

// ═══════════════════════════════════════════════════════════════════
//  Static Input::Get() — function-local static, thread-safe (C++11)
// ═══════════════════════════════════════════════════════════════════

Input& Input::Get() {
    static GLFWInput s_instance;
    return s_instance;
}

// ═══════════════════════════════════════════════════════════════════
//  GLFW window access (lazy-init + cache)
// ═══════════════════════════════════════════════════════════════════

GLFWwindow* GLFWInput::GetGLFWWindow() const {
    if (!m_window) {
        m_window = static_cast<GLFWwindow*>(
            Application::Get().GetWindow().GetNativeWindow());
    }
    return m_window;
}

// ═══════════════════════════════════════════════════════════════════
//  BeginFrame — snapshot current state as "previous" for edge detect
// ═══════════════════════════════════════════════════════════════════

void GLFWInput::BeginFrame() {
    m_prevKeyState         = m_keyState;
    m_prevMouseButtonState  = m_mouseButtonState;
    m_keyJustPressed.clear();
    m_mouseButtonJustPressed.clear();

    // Reset per-frame mouse delta; position tracking continues so the
    // next motion event resumes smoothly without a jump.
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;

    // Snapshot gamepad button states as "previous" for edge detection.
    // Gamepads have no press/release events, so we poll once per frame.
    for (int i = 0; i < kMaxGamepads; ++i) {
        GLFWgamepadstate state;
        if (glfwGetGamepadState(i, &state)) {
            for (int b = 0; b < static_cast<int>(GamepadButton::Count); ++b)
                m_prevGamepadButtons[i][b] = state.buttons[b] == GLFW_PRESS;
        } else {
            m_prevGamepadButtons[i].fill(false);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Key state — continuous (true every frame while held)
//  Polled directly from GLFW; authoritative regardless of event flow.
// ═══════════════════════════════════════════════════════════════════

bool GLFWInput::IsKeyPressed(KeyCode keycode) const {
    auto* window = GetGLFWWindow();
    return window && glfwGetKey(window, static_cast<int>(keycode)) == GLFW_PRESS;
}

// ═══════════════════════════════════════════════════════════════════
//  Key state — rising edge (true only on first press frame)
//
//  "Current" from GLFW (authoritative even if events are consumed).
//  "Previous" from BeginFrame snapshot (event-tracked state from last frame).
//  Rising edge = key is down now AND was NOT down last frame.
// ═══════════════════════════════════════════════════════════════════

bool GLFWInput::IsKeyJustPressed(KeyCode keycode) const {
    // Only true if OnEvent received KeyPressedEvent with repeatCount==0
    // (GLFW_PRESS action). GLFW_REPEAT events (repeatCount>=1) do NOT set this flag.
    auto* window = GetGLFWWindow();
    bool current = window && glfwGetKey(window, static_cast<int>(keycode)) == GLFW_PRESS;

    auto it = m_keyJustPressed.find(keycode);
    bool justPressed = it != m_keyJustPressed.end() && it->second;

    return current && justPressed;
}

// ═══════════════════════════════════════════════════════════════════
//  Mouse button state — continuous
// ═══════════════════════════════════════════════════════════════════

bool GLFWInput::IsMouseButtonPressed(MouseCode button) const {
    auto* window = GetGLFWWindow();
    return window && glfwGetMouseButton(window, static_cast<int>(button)) == GLFW_PRESS;
}

// ═══════════════════════════════════════════════════════════════════
//  Mouse button state — rising edge
// ═══════════════════════════════════════════════════════════════════

bool GLFWInput::IsMouseButtonJustPressed(MouseCode button) const {
    auto* window = GetGLFWWindow();
    bool current = window && glfwGetMouseButton(window, static_cast<int>(button)) == GLFW_PRESS;

    auto it = m_mouseButtonJustPressed.find(button);
    bool justPressed = it != m_mouseButtonJustPressed.end() && it->second;

    return current && justPressed;
}

// ═══════════════════════════════════════════════════════════════════
//  Mouse position — GLFW polling (no event tracking needed)
// ═══════════════════════════════════════════════════════════════════

float GLFWInput::GetMouseX() const {
    auto* window = GetGLFWWindow();
    if (!window) return 0.0f;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    return static_cast<float>(x);
}

float GLFWInput::GetMouseY() const {
    auto* window = GetGLFWWindow();
    if (!window) return 0.0f;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    return static_cast<float>(y);
}

// ── Mouse motion delta (per frame) ──────────────────────────────
float GLFWInput::GetMouseDeltaX() const { return m_mouseDeltaX; }
float GLFWInput::GetMouseDeltaY() const { return m_mouseDeltaY; }

// ── Gamepad (controller) state ───────────────────────────────────────
// glfwGetGamepadState succeeds only for present gamepads with a
// standard mapping, so every method is safe to call on any slot.
bool GLFWInput::IsGamepadPresent(int index) const {
    if (index < 0 || index >= kMaxGamepads)
        return false;
    return glfwJoystickPresent(index) && glfwJoystickIsGamepad(index);
}

std::string GLFWInput::GetGamepadName(int index) const {
    if (index < 0 || index >= kMaxGamepads)
        return {};
    const char* name = glfwGetGamepadName(index);
    return name ? std::string(name) : std::string{};
}

bool GLFWInput::IsGamepadButtonPressed(int index, GamepadButton button) const {
    if (index < 0 || index >= kMaxGamepads)
        return false;
    GLFWgamepadstate state;
    if (!glfwGetGamepadState(index, &state))
        return false;
    return state.buttons[static_cast<int>(button)] == GLFW_PRESS;
}

bool GLFWInput::IsGamepadButtonJustPressed(int index, GamepadButton button) const {
    if (index < 0 || index >= kMaxGamepads)
        return false;
    GLFWgamepadstate state;
    if (!glfwGetGamepadState(index, &state))
        return false;
    const int b = static_cast<int>(button);
    const bool current  = state.buttons[b] == GLFW_PRESS;
    const bool previous = m_prevGamepadButtons[index][b];
    return current && !previous;
}

float GLFWInput::GetGamepadAxis(int index, GamepadAxis axis) const {
    if (index < 0 || index >= kMaxGamepads)
        return 0.0f;
    GLFWgamepadstate state;
    if (!glfwGetGamepadState(index, &state))
        return 0.0f;
    return state.axes[static_cast<int>(axis)];
}

// ═══════════════════════════════════════════════════════════════════
//  OnEvent — track key/mouse state for frame-to-frame edge detection
//
//  GLFW callbacks fire per key transition, so we can reliably track
//  press/release events regardless of whether layers consume them.
//  This state is snapshotted by BeginFrame() into m_prev*State for
//  the edge-detection methods to compare against.
// ═══════════════════════════════════════════════════════════════════

void GLFWInput::OnEvent(Event& e) {
    EventDispatcher dispatcher(e);

    dispatcher.Dispatch<KeyPressedEvent>([](KeyPressedEvent& event) {
        auto& self = static_cast<GLFWInput&>(Input::Get());
        // GLFW_PRESS (repeatCount==0) → "just pressed" this frame
        // GLFW_REPEAT (repeatCount>=1) → held, do NOT re-trigger edge
        if (event.GetRepeatCount() == 0) {
            self.m_keyJustPressed[event.GetKeyCode()] = true;
        }
        self.m_keyState[event.GetKeyCode()] = true;
        return false;
    });

    dispatcher.Dispatch<KeyReleasedEvent>([](KeyReleasedEvent& event) {
        static_cast<GLFWInput&>(Input::Get()).m_keyState[event.GetKeyCode()] = false;
        return false;
    });

    dispatcher.Dispatch<MouseButtonPressedEvent>([](MouseButtonPressedEvent& event) {
        auto& self = static_cast<GLFWInput&>(Input::Get());
        self.m_mouseButtonJustPressed[event.GetMouseButton()] = true;
        self.m_mouseButtonState[event.GetMouseButton()] = true;
        return false;
    });

    dispatcher.Dispatch<MouseButtonReleasedEvent>([](MouseButtonReleasedEvent& event) {
        static_cast<GLFWInput&>(Input::Get()).m_mouseButtonState[event.GetMouseButton()] = false;
        return false;
    });

    // Mouse motion delta - accumulate per-event movement for GetMouseDelta.
    dispatcher.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& event) {
        const float x = event.GetX();
        const float y = event.GetY();
        if (!m_mousePosInitialized) {
            m_lastMouseX = x;
            m_lastMouseY = y;
            m_mousePosInitialized = true;
        } else {
            m_mouseDeltaX += x - m_lastMouseX;
            m_mouseDeltaY += y - m_lastMouseY;
            m_lastMouseX = x;
            m_lastMouseY = y;
        }
        return false;  // keep propagating (e.g. camera drag)
    });
}

} // namespace DMGameEngine
