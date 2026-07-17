#include "DMGameEngine/Platform/Windows/WindowsWindow.h"

#include "DMGameEngine/Core/Events/ApplicationEvent.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"
#include "DMGameEngine/Core/Events/GamepadEvent.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLGraphicsContext.h"
#include "DMGameEngine/Renderer/Renderer.h"
#ifdef DMGE_VULKAN
#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cstdlib>

namespace DMGameEngine {

// ── GLFW error callback ──────────────────────────────────────────
static void GLFWErrorCallback(int error, const char* description) {
    DMGE_LOG_ERROR("GLFW Error ({}): {}", error, description);
}

// Window whose callback chain receives global joystick / gamepad events.
// The GLFW joystick callback has no window parameter, so we bridge via
// this static pointer (the engine runs a single primary window).
static GLFWwindow* s_eventWindow = nullptr;

// ── Factory ──────────────────────────────────────────────────────
DM::Scope<Window> Window::Create(const WindowProps& props) {
    return DM::CreateScope<WindowsWindow>(props);
}

// ── Constructor / Destructor ─────────────────────────────────────
WindowsWindow::WindowsWindow(const WindowProps& props) {
    Init(props);
}

WindowsWindow::~WindowsWindow() {
    Shutdown();
}

// ── Init ─────────────────────────────────────────────────────────
void WindowsWindow::Init(const WindowProps& props) {
    m_data.title  = props.title;
    m_data.width  = props.width;
    m_data.height = props.height;

    DMGE_LOG_INFO("Creating window '{}' ({} x {})", props.title, props.width, props.height);

    // ── GLFW init ────────────────────────────────────────────────
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit()) {
        DMGE_LOG_CRITICAL("Failed to initialize GLFW");
        std::abort();
    }

    // Backend-specific window hints.
    if (Renderer::GetAPI() == Renderer::API::Vulkan)
    {
        // No OpenGL context is created for Vulkan; the VkSurfaceKHR is
        // created explicitly by VulkanGraphicsContext::Init().
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    else
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    m_window = glfwCreateWindow(
        static_cast<int>(props.width),
        static_cast<int>(props.height),
        props.title.c_str(),
        nullptr, nullptr);

    if (!m_window) {
        DMGE_LOG_CRITICAL("Failed to create GLFW window");
        glfwTerminate();
        std::abort();
    }

    // ── Store pointer to WindowData for use in callbacks ─────────
    glfwSetWindowUserPointer(m_window, &m_data);

    s_eventWindow = m_window;

    // ── OpenGL context via GraphicsContext abstraction ──────────
    if (Renderer::GetAPI() == Renderer::API::Vulkan)
    {
#ifdef DMGE_VULKAN
        m_context = DM::CreateScope<VulkanGraphicsContext>(m_window);
#else
        DMGE_CORE_ASSERT(false, "Vulkan backend not built (enable DMGE_VULKAN_BACKEND).");
#endif
    }
    else
    {
        m_context = DM::CreateScope<OpenGLGraphicsContext>(m_window);
    }
    m_context->Init();

    SetVSync(true);

    // ── Window callbacks ─────────────────────────────────────────
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int w, int h) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        data.width  = static_cast<unsigned int>(w);
        data.height = static_cast<unsigned int>(h);
        WindowResizeEvent event(data.width, data.height);
        data.callback(event);
    });

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        WindowCloseEvent event;
        data.callback(event);
    });

    glfwSetWindowFocusCallback(m_window, [](GLFWwindow* window, int focused) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (focused) {
            WindowFocusEvent event;
            data.callback(event);
        } else {
            WindowLostFocusEvent event;
            data.callback(event);
        }
    });

    // ── Keyboard callbacks ───────────────────────────────────────
    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        switch (action) {
        case GLFW_PRESS: {
            KeyPressedEvent event(static_cast<KeyCode>(key), 0);
            data.callback(event);
            break;
        }
        case GLFW_RELEASE: {
            KeyReleasedEvent event(static_cast<KeyCode>(key));
            data.callback(event);
            break;
        }
        case GLFW_REPEAT: {
            KeyPressedEvent event(static_cast<KeyCode>(key), 1);
            data.callback(event);
            break;
        }
        }
    });

    glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        KeyTypedEvent event(static_cast<KeyCode>(codepoint));
        data.callback(event);
    });

    // ── Mouse callbacks ──────────────────────────────────────────
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int /*mods*/) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        switch (action) {
        case GLFW_PRESS: {
            MouseButtonPressedEvent event(static_cast<MouseCode>(button));
            data.callback(event);
            break;
        }
        case GLFW_RELEASE: {
            MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
            data.callback(event);
            break;
        }
        }
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        MouseMovedEvent event(static_cast<float>(x), static_cast<float>(y));
        data.callback(event);
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
        data.callback(event);
    });

    // ── Joystick / gamepad hot-plug callback ──────────────────────────────
    glfwSetJoystickCallback([](int jid, int event) {
        if (!s_eventWindow)
            return;
        auto& data = *static_cast<WindowData*>(
            glfwGetWindowUserPointer(s_eventWindow));
        if (!data.callback)
            return;
        if (event == GLFW_CONNECTED) {
            const char* name = glfwGetJoystickName(jid);
            GamepadConnectedEvent ev(jid, name ? name : "");
            data.callback(ev);
        } else {  // GLFW_DISCONNECTED
            GamepadDisconnectedEvent ev(jid);
            data.callback(ev);
        }
    });
}

// ── Shutdown ─────────────────────────────────────────────────────
void WindowsWindow::Shutdown() {
    s_eventWindow = nullptr;

    // Graphics context must be destroyed before the GLFW window,
    // since it holds references to the window's GL context.
    m_context.reset();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

// ── PollEvents ───────────────────────────────────────────────────
void WindowsWindow::PollEvents() {
    glfwPollEvents();
}

// ── SwapBuffers ────────────────────────────────────────────────────
void WindowsWindow::SwapBuffers() {
    m_context->SwapBuffers();
}

// ── Native Window ────────────────────────────────────────────────
void* WindowsWindow::GetNativeWindow() const {
    return static_cast<void*>(m_window);
}

// ── VSync ────────────────────────────────────────────────────────
void WindowsWindow::SetVSync(bool enabled) {
    m_data.vSync = enabled;
    if (Renderer::GetAPI() != Renderer::API::Vulkan)
        glfwSwapInterval(enabled ? 1 : 0);  // VSync for Vulkan is handled by the present mode.
}

// ── Cursor mode ─────────────────────────────────────────────────
void WindowsWindow::SetCursorMode(CursorMode mode) {
    m_cursorMode = mode;
    int glfwMode = GLFW_CURSOR_NORMAL;
    switch (mode) {
    case CursorMode::Normal:   glfwMode = GLFW_CURSOR_NORMAL;   break;
    case CursorMode::Hidden:   glfwMode = GLFW_CURSOR_HIDDEN;   break;
    case CursorMode::Disabled: glfwMode = GLFW_CURSOR_DISABLED; break;
    }
    glfwSetInputMode(m_window, GLFW_CURSOR, glfwMode);
}

// ── Raw mouse motion ────────────────────────────────────────────────
void WindowsWindow::SetRawMouseMotion(bool enabled) {
    m_rawMouseMotion = enabled;
    // Raw motion only takes effect with the cursor disabled and needs
    // platform support; otherwise the request is recorded but inert.
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION,
                         enabled ? GLFW_TRUE : GLFW_FALSE);
}

} // namespace DMGameEngine
