#include "DMGameEngine/Platform/Windows/WindowsWindow.h"

#include "DMGameEngine/Core/Events/ApplicationEvent.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Platform/OpenGL/OpenGLGraphicsContext.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cstdlib>

namespace DMGameEngine {

// ── GLFW error callback ──────────────────────────────────────────
static void GLFWErrorCallback(int error, const char* description) {
    DMGE_LOG_ERROR("GLFW Error ({}): {}", error, description);
}

// ── Factory ──────────────────────────────────────────────────────
std::unique_ptr<Window> Window::Create(const WindowProps& props) {
    return std::make_unique<WindowsWindow>(props);
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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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

    // ── OpenGL context via GraphicsContext abstraction ──────────
    m_context = std::make_unique<OpenGLGraphicsContext>(m_window);
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
}

// ── Shutdown ─────────────────────────────────────────────────────
void WindowsWindow::Shutdown() {
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
    glfwSwapInterval(enabled ? 1 : 0);
}

} // namespace DMGameEngine
