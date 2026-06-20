/*
 * DMGameEngine - Keyboard Events
 *
 * KeyPressed, KeyReleased, KeyTyped events with key codes.
 */

#pragma once

#include "DMGameEngine/Core/Events/Event.h"

#include <sstream>

namespace DMGameEngine {

// ── Key Codes ────────────────────────────────────────────────────
enum class KeyCode : int {
    // Printable keys
    Space = 32,
    Apostrophe = 39,    // '
    Comma = 44,         // ,
    Minus = 45,         // -
    Period = 46,        // .
    Slash = 47,         // /
    Key0 = 48, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
    Semicolon = 59,     // ;
    Equal = 61,         // =
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    LeftBracket = 91,   // [
    Backslash = 92,     // \
    RightBracket = 93,  // ]
    GraveAccent = 96,   // `

    // Function keys
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,

    // Keypad
    KP0 = 320, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
    KPDecimal = 330,
    KPDivide = 331,
    KPMultiply = 332,
    KPSubtract = 333,
    KPAdd = 334,
    KPEnter = 335,
    KPEqual = 336,

    // Modifiers
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,
};

// ── KeyEvent (base for key events) ───────────────────────────────
class DMGE_API KeyEvent : public Event {
public:
    KeyCode GetKeyCode() const { return m_keyCode; }

    EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)

protected:
    explicit KeyEvent(KeyCode keyCode) : m_keyCode(keyCode) {}
    KeyCode m_keyCode;
};

// ── KeyPressedEvent ──────────────────────────────────────────────
class DMGE_API KeyPressedEvent : public KeyEvent {
public:
    KeyPressedEvent(KeyCode keyCode, int repeatCount)
        : KeyEvent(keyCode), m_repeatCount(repeatCount) {}

    int GetRepeatCount() const { return m_repeatCount; }

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "KeyPressedEvent: " << static_cast<int>(m_keyCode)
            << " (repeat=" << m_repeatCount << ")";
        return oss.str();
    }

    EVENT_CLASS_TYPE(KeyPressed)

private:
    int m_repeatCount;
};

// ── KeyReleasedEvent ─────────────────────────────────────────────
class DMGE_API KeyReleasedEvent : public KeyEvent {
public:
    explicit KeyReleasedEvent(KeyCode keyCode) : KeyEvent(keyCode) {}

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "KeyReleasedEvent: " << static_cast<int>(m_keyCode);
        return oss.str();
    }

    EVENT_CLASS_TYPE(KeyReleased)
};

// ── KeyTypedEvent ────────────────────────────────────────────────
class DMGE_API KeyTypedEvent : public KeyEvent {
public:
    explicit KeyTypedEvent(KeyCode keyCode) : KeyEvent(keyCode) {}

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "KeyTypedEvent: " << static_cast<int>(m_keyCode);
        return oss.str();
    }

    EVENT_CLASS_TYPE(KeyTyped)
};

} // namespace DMGameEngine
