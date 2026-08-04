/*
 * DMGameEngine - Gamepad Events
 *
 * GamepadConnected / GamepadDisconnected events, fired when a
 * controller is hot-plugged or removed. The jid identifies the
 * controller slot; use Input::IsGamepadPresent() to check whether
 * the slot holds a usable gamepad with a standard mapping.
 */

#pragma once

#include "DMGameEngine/Core/Events/Event.h"

#include <sstream>
#include <string>

namespace DMGameEngine {

// ── GamepadConnectedEvent ───────────────────────────────────────
class DMGE_API GamepadConnectedEvent : public Event {
public:
    GamepadConnectedEvent(int jid, std::string name)
        : m_jid(jid), m_name(std::move(name)) {}

    int GetJid() const { return m_jid; }
    const std::string& GetGamepadName() const { return m_name; }

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "GamepadConnectedEvent: " << m_jid
            << " ('" << (m_name.empty() ? "Unknown" : m_name) << "')";
        return oss.str();
    }

    EVENT_CLASS_TYPE(GamepadConnected)
    EVENT_CLASS_CATEGORY(EventCategory::Gamepad | EventCategory::Input)

private:
    int         m_jid;
    std::string m_name;
};

// ── GamepadDisconnectedEvent ────────────────────────────────────
class DMGE_API GamepadDisconnectedEvent : public Event {
public:
    explicit GamepadDisconnectedEvent(int jid) : m_jid(jid) {}

    int GetJid() const { return m_jid; }

    std::string ToString() const override {
        std::ostringstream oss;
        oss << "GamepadDisconnectedEvent: " << m_jid;
        return oss.str();
    }

    EVENT_CLASS_TYPE(GamepadDisconnected)
    EVENT_CLASS_CATEGORY(EventCategory::Gamepad | EventCategory::Input)

private:
    int m_jid;
};

} // namespace DMGameEngine