/*
 * DMGameEngine - OpenGL Graphics Context Implementation
 */

#include "DMGameEngine/Platform/OpenGL/OpenGLGraphicsContext.h"

#include "DMGameEngine/Core/Log.h"

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace DMGameEngine {

OpenGLGraphicsContext::OpenGLGraphicsContext(GLFWwindow* windowHandle)
    : m_WindowHandle(windowHandle)
{
}

void OpenGLGraphicsContext::Init()
{
    glfwMakeContextCurrent(m_WindowHandle);

    int status = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
    DMGE_CORE_ASSERT(status, "Failed to initialize Glad!");

    DMGE_LOG_INFO("OpenGL Info:");
    DMGE_LOG_INFO("  Vendor:   {0}", GetVendor());
    DMGE_LOG_INFO("  Renderer: {0}", GetRenderer());
    DMGE_LOG_INFO("  Version:  {0}", GetVersion());
}

void OpenGLGraphicsContext::SwapBuffers()
{
    glfwSwapBuffers(m_WindowHandle);
}

const char* OpenGLGraphicsContext::GetVendor() const
{
    return reinterpret_cast<const char*>(glGetString(GL_VENDOR));
}

const char* OpenGLGraphicsContext::GetRenderer() const
{
    return reinterpret_cast<const char*>(glGetString(GL_RENDERER));
}

const char* OpenGLGraphicsContext::GetVersion() const
{
    return reinterpret_cast<const char*>(glGetString(GL_VERSION));
}

} // namespace DMGameEngine
