/*
 * DMGameEngine - Keyboard Events
 *
 * KeyPressed, KeyReleased, KeyTyped events with key codes.
 */

#pragma once

#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/KeyCodes.h"

#include <sstream>

namespace DMGameEngine {

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
