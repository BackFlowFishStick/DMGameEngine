/*
 * DMGameEngine - OpenGL Graphics Context
 *
 * OpenGL implementation of the GraphicsContext abstraction.
 */

#pragma once

#include "DMGameEngine/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace DMGameEngine {

class DMGE_API OpenGLGraphicsContext : public GraphicsContext
{
public:
    explicit OpenGLGraphicsContext(GLFWwindow* windowHandle);

    void Init() override;
    void SwapBuffers() override;

    const char* GetVendor() const override;
    const char* GetRenderer() const override;
    const char* GetVersion() const override;

private:
    GLFWwindow* m_WindowHandle = nullptr;
};

} // namespace DMGameEngine
