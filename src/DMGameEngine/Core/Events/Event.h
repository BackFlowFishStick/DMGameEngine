/*
 * DMGameEngine - Event System Core
 *
 * Defines the base Event class, event type and category enums,
 * and the EventDispatcher for routing events to handlers.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

#include <sstream>
#include <string>

namespace DMGameEngine {

// ── Bit helper ───────────────────────────────────────────────────
#define BIT(x) (1 << (x))

// ── Event Types ──────────────────────────────────────────────────
enum class EventType {
    None = 0,

    // Window
    WindowClose,
    WindowResize,
    WindowFocus,
    WindowLostFocus,
    WindowMoved,

    // Application
    AppTick,
    AppUpdate,
    AppRender,

    // Keyboard
    KeyPressed,
    KeyReleased,
    KeyTyped,

    // Mouse
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled,
};

// ── Event Categories (bitmask for filtering) ─────────────────────
enum EventCategory {
    None        = 0,
    Application = BIT(0),
    Input       = BIT(1),
    Keyboard    = BIT(2),
    Mouse       = BIT(3),
    MouseButton = BIT(4),
};

// ── Macro to reduce boilerplate in derived event classes ─────────
#define EVENT_CLASS_TYPE(type)                                          \
    static EventType GetStaticType() { return EventType::type; }        \
    EventType GetEventType() const override { return GetStaticType(); } \
    const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) \
    int GetCategoryFlags() const override { return category; }

// ── Base Event ───────────────────────────────────────────────────
class DMGE_API Event {
public:
    virtual ~Event() = default;

    virtual EventType GetEventType() const   = 0;
    virtual const char* GetName() const      = 0;
    virtual int GetCategoryFlags() const     = 0;
    virtual std::string ToString() const { return GetName(); }

    bool IsInCategory(EventCategory category) const {
        return GetCategoryFlags() & category;
    }

    bool Handled = false;
};

// ── Event Dispatcher ─────────────────────────────────────────────
class EventDispatcher {
public:
    explicit EventDispatcher(Event& event)
        : m_event(event) {}

    // Dispatch to a handler function of signature  bool(T&)
    template<typename T, typename F>
    bool Dispatch(const F& func) {
        if (m_event.GetEventType() == T::GetStaticType()) {
            m_event.Handled |= func(static_cast<T&>(m_event));
            return true;
        }
        return false;
    }

private:
    Event& m_event;
};

// ── Inline ToString helpers ──────────────────────────────────────
inline std::string ToString(const EventType type) {
    switch (type) {
    case EventType::WindowClose:          return "WindowClose";
    case EventType::WindowResize:         return "WindowResize";
    case EventType::WindowFocus:          return "WindowFocus";
    case EventType::WindowLostFocus:      return "WindowLostFocus";
    case EventType::WindowMoved:          return "WindowMoved";
    case EventType::AppTick:              return "AppTick";
    case EventType::AppUpdate:            return "AppUpdate";
    case EventType::AppRender:            return "AppRender";
    case EventType::KeyPressed:           return "KeyPressed";
    case EventType::KeyReleased:          return "KeyReleased";
    case EventType::KeyTyped:             return "KeyTyped";
    case EventType::MouseButtonPressed:   return "MouseButtonPressed";
    case EventType::MouseButtonReleased:  return "MouseButtonReleased";
    case EventType::MouseMoved:           return "MouseMoved";
    case EventType::MouseScrolled:        return "MouseScrolled";
    default:                              return "Unknown";
    }
}

} // namespace DMGameEngine

#undef BIT
