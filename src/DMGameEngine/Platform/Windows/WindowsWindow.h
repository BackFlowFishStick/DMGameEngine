/*
 * DMGameEngine - Windows Window (GLFW Implementation)
 */

#pragma once

#include "DMGameEngine/Core/Window.h"
#include "DMGameEngine/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace DMGameEngine {

class WindowsWindow : public Window {
public:
    explicit WindowsWindow(const WindowProps& props);
    ~WindowsWindow() override;

    void PollEvents() override;
    void SwapBuffers() override;

    unsigned int GetWidth()       const override { return m_data.width; }
    unsigned int GetHeight()      const override { return m_data.height; }
    void*        GetNativeWindow() const override;

    void SetEventCallback(const EventCallbackFn& callback) override {
        m_data.callback = callback;
    }
    void SetVSync(bool enabled) override;
    bool IsVSync() const override { return m_data.vSync; }

    void SetCursorMode(CursorMode mode) override;
    CursorMode GetCursorMode() const override { return m_cursorMode; }
    void SetRawMouseMotion(bool enabled) override;
    bool IsRawMouseMotion() const override { return m_rawMouseMotion; }

private:
    void Init(const WindowProps& props);
    void Shutdown();

    GLFWwindow* m_window = nullptr;

    CursorMode m_cursorMode     = CursorMode::Normal;
    bool       m_rawMouseMotion = false;

    struct WindowData {
        std::string     title;
        unsigned int    width  = 0;
        unsigned int    height = 0;
        bool            vSync  = false;
        EventCallbackFn callback;
    };

    WindowData m_data;

    DM::Scope<GraphicsContext> m_context;
};

} // namespace DMGameEngine
