/*
 * DMGameEngine - Renderer Abstraction
 *
 * High-level renderer API. Manages scene submission and
 * delegates to the active GraphicsContext backend.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

namespace DMGameEngine {

class DMGE_API Renderer
{
public:
    enum class API
    {
        None = 0,
        OpenGL,
        Vulkan,
        DirectX
    };

public:
    static void Init();
    static void Shutdown();

    static void BeginScene();
    static void EndScene();

    static void Submit(const class VertexArray& vertexArray);
    static void Flush();

    static void OnWindowResize(int width, int height);

    static API GetAPI() { return s_API; }

private:
    static API s_API;
};

} // namespace DMGameEngine
