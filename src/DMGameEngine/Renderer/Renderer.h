/*
 * DMGameEngine - Renderer Abstraction
 *
 * High-level renderer API. Manages scene submission and delegates
 * low-level draw calls to the active RendererAPI backend.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"
#include <memory>

namespace DMGameEngine {

class RendererAPI; // forward declaration - backend instance owned below
class Camera;      // forward declaration - scene view-projection source
class Shader;      // forward declaration - bound per draw submission
class VertexArray; // forward declaration - vertex inputs for a draw

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
    // Clears the framebuffer and caches the camera's view-projection matrix
    // for subsequent Submit() calls (fed to shaders as u_ViewProjection).
    static void BeginScene(const Camera& camera);
    static void EndScene();
    static void SetClearColor(const glm::vec4& color);
    static void Clear();

    // Binds the shader, uploads u_ViewProjection (from BeginScene's camera)
    // and u_Transform, then issues an indexed draw for the vertex array.
    static void Submit(const std::shared_ptr<Shader>& shader,
                      const std::shared_ptr<VertexArray>& vertexArray,
                      const glm::mat4& transform = glm::mat4(1.0f));
    static void Flush();

    static void OnWindowResize(int width, int height);

    static API GetAPI() { return s_API; }

private:
    struct SceneData
    {
        glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
    };

    static API s_API;
    static std::unique_ptr<RendererAPI> s_RendererAPI;
    static SceneData s_SceneData;
};

} // namespace DMGameEngine
