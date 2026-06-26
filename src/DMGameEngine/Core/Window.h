/*
 * DMGameEngine - Window Abstraction
 *
 * Platform-agnostic window interface. Use Window::Create() to
 * get a platform-specific implementation.
 */

#pragma once

#include "DMGameEngine/Core/Events/Event.h"

#include <functional>
#include <memory>
#include <string>

namespace DMGameEngine {

// ── Window Properties ────────────────────────────────────────────
struct WindowProps {
    std::string  title;
    unsigned int width;
    unsigned int height;

    explicit WindowProps(const std::string& t  = "DMGameEngine",
                         unsigned int      w  = 1280,
                         unsigned int      h  = 720)
        : title(t), width(w), height(h) {}
};

// ── Window Interface ─────────────────────────────────────────────
class DMGE_API Window {
public:
    using EventCallbackFn = std::function<void(Event&)>;

    virtual ~Window() = default;

    // ── Event pump ────────────────────────────────────────
    // Polls the OS event queue and dispatches through the
    // registered callback. Must be called first each frame,
    // before any layer updates, to provide current input state.
    virtual void PollEvents() = 0;

    virtual unsigned int GetWidth()       const = 0;
    virtual unsigned int GetHeight()      const = 0;
    virtual void*        GetNativeWindow() const = 0;

    virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
    virtual void SetVSync(bool enabled) = 0;
    virtual bool IsVSync() const = 0;

    // Factory — returns a platform-specific Window instance
    static std::unique_ptr<Window> Create(const WindowProps& props = WindowProps());
};

} // namespace DMGameEngine
