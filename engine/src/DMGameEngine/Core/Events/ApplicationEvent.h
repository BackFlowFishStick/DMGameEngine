/*
 * DMGameEngine - Application Events
 *
 * Window resize/close/focus and App tick/update/render events.
 */

#pragma once

#include "DMGameEngine/Core/Events/Event.h"

#include <sstream>

namespace DMGameEngine {

// ── WindowResizeEvent ────────────────────────────────────────────
class DMGE_API WindowResizeEvent : public Event {
public:
    WindowResizeEvent(unsigned int width, unsigned int height)
        : m_width(width), m_height(height) {}

    unsigned int GetWidth()  const { return m_width; }
    unsigned int GetHeight() const { return m_height; }

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "WindowResizeEvent: " << m_width << " x " << m_height;
        return oss.str();
    }

    EVENT_CLASS_TYPE(WindowResize)
    EVENT_CLASS_CATEGORY(EventCategory::Application)

private:
    unsigned int m_width, m_height;
};

// ── WindowCloseEvent ─────────────────────────────────────────────
class DMGE_API WindowCloseEvent : public Event {
public:
    WindowCloseEvent() = default;

    EVENT_CLASS_TYPE(WindowClose)
    EVENT_CLASS_CATEGORY(EventCategory::Application)
};

// ── WindowFocusEvent ─────────────────────────────────────────────
class DMGE_API WindowFocusEvent : public Event {
public:
    WindowFocusEvent() = default;

    EVENT_CLASS_TYPE(WindowFocus)
    EVENT_CLASS_CATEGORY(EventCategory::Application)
};

// ── WindowLostFocusEvent ─────────────────────────────────────────
class DMGE_API WindowLostFocusEvent : public Event {
public:
    WindowLostFocusEvent() = default;

    EVENT_CLASS_TYPE(WindowLostFocus)
    EVENT_CLASS_CATEGORY(EventCategory::Application)
};

// ── WindowMovedEvent ─────────────────────────────────────────────
class DMGE_API WindowMovedEvent : public Event {
public:
    WindowMovedEvent(int x, int y) : m_x(x), m_y(y) {}

    int GetX() const { return m_x; }
    int GetY() const { return m_y; }

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "WindowMovedEvent: " << m_x << ", " << m_y;
        return oss.str();
    }

    EVENT_CLASS_TYPE(WindowMoved)
    EVENT_CLASS_CATEGORY(EventCategory::Application)

private:
    int m_x, m_y;
};

// ── AppTickEvent ─────────────────────────────────────────────────
class DMGE_API AppTickEvent : public Event {
public:
    AppTickEvent() = default;

    EVENT_CLASS_TYPE(AppTick)
    EVENT_CLASS_CATEGORY(EventCategory::Application)
};

// ── AppUpdateEvent ───────────────────────────────────────────────
class DMGE_API AppUpdateEvent : public Event {
public:
    AppUpdateEvent() = default;

    EVENT_CLASS_TYPE(AppUpdate)
    EVENT_CLASS_CATEGORY(EventCategory::Application)
};

// ── AppRenderEvent ───────────────────────────────────────────────
class DMGE_API AppRenderEvent : public Event {
public:
    AppRenderEvent() = default;

    EVENT_CLASS_TYPE(AppRender)
    EVENT_CLASS_CATEGORY(EventCategory::Application)
};

} // namespace DMGameEngine
