/*
 * DMGameEngine - Renderer (Scene Submission)
 *
 * High-level renderer surface. The host clears the framebuffer once per
 * frame via ClearFrame(); each scene layer brackets its own render pass
 * with BeginScene / EndScene, caching the camera view-projection for the
 * pass. Submit() enqueues draw requests into a per-pass RenderQueue
 * rather than issuing them immediately; the queue is sorted by
 * material/shader and flushed once at EndScene() (or on an explicit
 * Flush()), so each group binds its shader + uniforms only once per pass
 * instead of once per draw, and u_ViewProjection is uploaded once per
 * shader.
 *
 * Low-level GPU commands (clear, viewport, blend / depth / cull state,
 * draw calls) are owned by RenderCommand, which holds the active
 * RendererAPI backend. Renderer delegates to it rather than holding the
 * backend instance itself.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/DeferredRendering.h"  // RenderPath / DeferredPathState / DeferredShaderSet
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/RenderQueue.h"
#include "DMGameEngine/Renderer/Light.h"
#include "glm/glm.hpp"
#include <memory>

namespace DMGameEngine {

class Camera;      // forward declaration - scene view-projection source
class Shader;      // forward declaration - enqueued per draw submission
class Material;    // forward declaration - shader + uniform bundle per draw
class VertexArray; // forward declaration - vertex inputs for a draw
class FrameBuffer; // forward declaration - optional render target (offscreen pass)

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

    // Clears the framebuffer (color + depth) once per frame. Called by
    // the host (Application) before scene layers render. Separate from
    // BeginScene so multiple scene layers can each run a render pass
    // (BeginScene/EndScene) without wiping earlier passes' output.
    static void ClearFrame();

    // Begins a render pass. target selects the render target: nullptr (or
    // a FrameBuffer with SwapChainTarget) renders to the swapchain / default
    // framebuffer; an offscreen FrameBuffer renders into it (render-to-texture)
    // so multi-pass techniques (shadow maps, post-process, viewports) can run.
    static void BeginScene(const DM::Ref<FrameBuffer>& target = nullptr);
    // Convenience: begin a pass for the given camera, rendering into the
    // camera's own render target when one is configured (Camera::GetRenderTarget),
    // so offscreen rendering only needs the camera set up - no explicit target
    // argument. Delegates to the explicit BeginScene(camera, target) below.
    static void BeginScene(const Camera& camera);

    // Underlying implementation: caches the camera view-projection and resets
    // the per-pass RenderQueue for subsequent Submit() calls. target selects
    // the render target explicitly (it overrides the camera's); nullptr renders
    // to the swapchain / default framebuffer. The framebuffer is NOT cleared
    // here - ClearFrame handles that once per frame so multi-pass rendering
    // keeps earlier output.
    static void BeginScene(const Camera& camera, const DM::Ref<FrameBuffer>& target);
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

    // Enqueues a material-bound skinned draw (animation stage 1): identical to
    // Submit(material, ...) plus a per-draw bone palette uploaded as the
    // u_BoneMatrices uniform array at flush time. bonePalette must stay valid
    // until EndScene/Flush (it normally points into AnimatorComponent::Palette).
    // OpenGL only in stage 1 - see Shader::SetMat4Array for the Vulkan note.
    static void SubmitSkinned(const DM::Ref<Material>& material,
                              const DM::Ref<VertexArray>& vertexArray,
                              const glm::mat4& transform,
                              const glm::mat4* bonePalette, uint32_t bonePaletteCount);

    // Enqueues an instanced batch: one DrawIndexedInstanced for
    // instanceCount instances sharing the same material + VA.
    static void SubmitInstanced(const DM::Ref<Material>& material,
                                const DM::Ref<VertexArray>& vertexArray,
                                uint32_t instanceCount);

    // Flushes the per-frame RenderQueue: sorts by material/shader and
    // submits every queued draw, binding each group's state only once.
    // Called automatically by EndScene(); can also be called mid-frame
    // to submit the queue so far (e.g. between render passes).
    static void Flush();

    // Uploads aggregated light data for the current frame. Called by
    // LightSystem::OnRender before EndScene flushes the render queue.
    static void SubmitLightData(const SceneLightData& data);

    static void OnWindowResize(int width, int height);

    // ── Render path (3e deferred rendering, stage 1) ────────────
    // Selects Forward (default) or Deferred (G-buffer pass + full-screen
    // lighting pass; see documents/DEFERRED_RENDERING_DESIGN.md). The
    // switch takes effect at the NEXT BeginScene - a frame that already
    // started always completes under the path it began with (mid-frame
    // switches are ignored by design). Deferred resources (G-buffer
    // FrameBuffer, internal shaders, full-screen quad) are created lazily
    // on the first deferred BeginScene, follow the render target's size,
    // and are released by SetAPI / Shutdown.
    static void        SetRenderPath(RenderPath path);
    static RenderPath  GetRenderPath() { return s_PathState.Requested; }
    // Frame-locked snapshot (what the current/last frame actually ran under).
    static RenderPath  GetActiveRenderPath() { return s_PathState.Active; }

    static API  GetAPI()      { return s_API; }
    static void SetAPI(API api)
    {
        // Backend-owning resources (deferred shaders / G-buffer / quad) are
        // API-specific: release them when the API changes. Cheap when the
        // deferred path was never used (no-op before first deferred frame).
        if (api != s_API)
            DestroyDeferredResources();
        s_API = api;
    }

private:
    struct SceneData
    {
        glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
        glm::vec3 CameraPosition       = glm::vec3(0.0f);
    };

    static API s_API;
    static SceneData    s_SceneData;
    static SceneLightData s_LightData;
    static RenderQueue s_Queue;
    static DeferredPathState s_PathState;

    // ── Deferred-path orchestration (3e stage 1) ────────────────
    // Implemented in Renderer.cpp; the resource bundle (G-buffer
    // FrameBuffer, internal shaders, full-screen quad) lives as
    // file-statics there so the public header stays free of
    // implementation types (R1/R5).
    static void EnsureDeferredResources(uint32_t width, uint32_t height);
    static void DrawDeferredLighting();
    static void DestroyDeferredResources();
};

} // namespace DMGameEngine
