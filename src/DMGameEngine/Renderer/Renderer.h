/*
 * DMGameEngine - Renderer (Scene Submission)
 *
 * High-level renderer surface. Brackets a frame with BeginScene /
 * EndScene, caching the camera view-projection for the frame.
 * Submit() enqueues draw requests into a per-frame RenderQueue rather
 * than issuing them immediately; the queue is sorted by material/shader
 * and flushed once at EndScene() (or on an explicit Flush()), so each
 * group binds its shader + uniforms only once per frame instead of once
 * per draw, and u_ViewProjection is uploaded once per shader.
 *
 * Low-level GPU commands (clear, viewport, blend / depth / cull state,
 * draw calls) are owned by RenderCommand, which holds the active
 * RendererAPI backend. Renderer delegates to it rather than holding the
 * backend instance itself.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/RenderQueue.h"
#include "glm/glm.hpp"
#include <memory>

namespace DMGameEngine {

class Camera;      // forward declaration - scene view-projection source
class Shader;      // forward declaration - enqueued per draw submission
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
    // The per-frame RenderQueue is also cleared here.
    static void BeginScene(const Camera& camera);
    static void EndScene();

    // Enqueues a shader-bound draw into the per-frame RenderQueue. The
    // shader is bound and u_ViewProjection / u_Transform are uploaded
    // once per shader group when the queue is flushed, not per Submit.
    static void Submit(const DM::Ref<Shader>& shader,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform = glm::mat4(1.0f));
    // Enqueues a material-bound draw. The material (its shader + stored
    // uniforms) binds once per material group at flush time.
    static void Submit(const DM::Ref<Material>& material,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform = glm::mat4(1.0f));

    // Flushes the per-frame RenderQueue: sorts by material/shader and
    // submits every queued draw, binding each group's state only once.
    // Called automatically by EndScene(); can also be called mid-frame
    // to submit the queue so far (e.g. between render passes).
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
    static RenderQueue s_Queue;
};

} // namespace DMGameEngine