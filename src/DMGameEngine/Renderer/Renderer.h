/*
 * DMGameEngine - Renderer (Scene Submission)
 *
 * High-level renderer surface. Brackets a frame with BeginScene /
 * EndScene, caching the camera view-projection for the frame, and
 * Submit() binds the shader + uniforms before issuing a draw.
 *
 * Low-level GPU commands (clear, viewport, blend / depth / cull
 * state, draw calls) are owned by RenderCommand, which holds the
 * active RendererAPI backend. Renderer delegates to it rather than
 * holding the backend instance itself.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "glm/glm.hpp"
#include <memory>

namespace DMGameEngine {

class Camera;      // forward declaration - scene view-projection source
class Shader;      // forward declaration - bound per draw submission
class Material;    // forward declaration - shader + uniform bundle per draw
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
    static void Init(const RendererAPIInitConfig& config = {});
    static void Shutdown();

    static void BeginScene();
    // Clears the framebuffer and caches the camera's view-projection matrix
    // for subsequent Submit() calls (fed to shaders as u_ViewProjection).
    static void BeginScene(const Camera& camera);
    static void EndScene();

    // Binds the shader, uploads u_ViewProjection (from BeginScene's camera)
    // and u_Transform, then issues an indexed draw for the vertex array.
    static void Submit(const DM::Ref<Shader>& shader,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform = glm::mat4(1.0f));
    // Binds the material (its shader + stored uniforms), then uploads
    // u_ViewProjection (cached by BeginScene) and u_Transform before
    // issuing an indexed draw. Use this when a draw carries per-material
    // uniform values (e.g. u_Color).
    static void Submit(const DM::Ref<Material>& material,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform = glm::mat4(1.0f));
    static void Flush();

    static void OnWindowResize(int width, int height);

    static API  GetAPI()      { return s_API; }
    static void SetAPI(API api) { s_API = api; }

private:
    struct SceneData
    {
        glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
    };

    static API s_API;
    static SceneData s_SceneData;
};

} // namespace DMGameEngine