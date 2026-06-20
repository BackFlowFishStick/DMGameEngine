/*
 * DMGameEngine - Mouse Events
 *
 * MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled events.
 */

#pragma once

#include "DMGameEngine/Core/Events/Event.h"

#include <sstream>

namespace DMGameEngine {

// ── Mouse Button Codes ───────────────────────────────────────────
enum class MouseCode : int {
    Button0 = 0,
    Button1 = 1,
    Button2 = 2,
    Button3 = 3,
    Button4 = 4,
    Button5 = 5,
    Button6 = 6,
    Button7 = 7,

    Left   = Button0,
    Right  = Button1,
    Middle = Button2,
};

// ── MouseMovedEvent ──────────────────────────────────────────────
class DMGE_API MouseMovedEvent : public Event {
public:
    MouseMovedEvent(float x, float y) : m_mouseX(x), m_mouseY(y) {}

    float GetX() const { return m_mouseX; }
    float GetY() const { return m_mouseY; }

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "MouseMovedEvent: " << m_mouseX << ", " << m_mouseY;
        return oss.str();
    }

    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input)

private:
    float m_mouseX, m_mouseY;
};

// ── MouseScrolledEvent ───────────────────────────────────────────
class DMGE_API MouseScrolledEvent : public Event {
public:
    MouseScrolledEvent(float xOffset, float yOffset)
        : m_xOffset(xOffset), m_yOffset(yOffset) {}

    float GetXOffset() const { return m_xOffset; }
    float GetYOffset() const { return m_yOffset; }

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "MouseScrolledEvent: " << m_xOffset << ", " << m_yOffset;
        return oss.str();
    }

    EVENT_CLASS_TYPE(MouseScrolled)
    EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input)

private:
    float m_xOffset, m_yOffset;
};

// ── MouseButtonEvent (base for button events) ────────────────────
class DMGE_API MouseButtonEvent : public Event {
public:
    MouseCode GetMouseButton() const { return m_button; }

    EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input | EventCategory::MouseButton)

protected:
    explicit MouseButtonEvent(MouseCode button) : m_button(button) {}
    MouseCode m_button;
};

// ── MouseButtonPressedEvent ──────────────────────────────────────
class DMGE_API MouseButtonPressedEvent : public MouseButtonEvent {
public:
    explicit MouseButtonPressedEvent(MouseCode button) : MouseButtonEvent(button) {}

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "MouseButtonPressedEvent: " << static_cast<int>(m_button);
        return oss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
};

// ── MouseButtonReleasedEvent ─────────────────────────────────────
class DMGE_API MouseButtonReleasedEvent : public MouseButtonEvent {
public:
    explicit MouseButtonReleasedEvent(MouseCode button) : MouseButtonEvent(button) {}

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "MouseButtonReleasedEvent: " << static_cast<int>(m_button);
        return oss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
};

} // namespace DMGameEngine
