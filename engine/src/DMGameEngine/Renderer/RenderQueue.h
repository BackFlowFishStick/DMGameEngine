/*
 * DMGameEngine - Render Queue (Deferred Submission + State Sorting)
 *
 * Collects draw requests throughout a frame and submits them in a single
 * sorted pass at Flush(). Renderables sharing the same Material (or, for
 * the legacy path, the same Shader) are grouped together so the shader +
 * material uniforms bind once per group, and the per-frame
 * view-projection is uploaded once per shader rather than once per draw.
 *
 * Supports two paths: per-draw (DrawIndexed) for small / transparent groups,
 * and instanced (DrawIndexedInstanced) for large groups sharing the same
 * state binds and uniform uploads, leaving the draw-call count unchanged
 * but cutting per-draw CPU/GPU state churn.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/DeferredRendering.h"  // DeferredShaderSet for FlushDeferred
#include "glm/glm.hpp"
#include <cstddef>
#include <vector>
#include "DMGameEngine/Renderer/Light.h"  // SceneLightData for Flush

namespace DMGameEngine {

class Material;    // forward declaration - shader + uniform bundle
class Shader;      // forward declaration - fallback when no Material
class VertexArray; // forward declaration - vertex inputs for a draw

// ── Deferred draw request ───────────────────────────────────────
//
// Either Material or Shader must be set. Material takes precedence and
// is the preferred path because it carries per-material uniforms; the
// Shader-only field supports the legacy Submit(shader, ...).
struct Renderable
{
    DM::Ref<Material>    Material;                  // optional (preferred)
    DM::Ref<Shader>      Shader;                    // used when Material is null
    DM::Ref<VertexArray> VertexArray;
    glm::mat4            Transform = glm::mat4(1.0f);
    // Skinning (animation stage 1): per-draw bone palette uploaded as the
    // u_BoneMatrices uniform array. Pointer into the AnimatorComponent's
    // Palette vector - valid for the frame between Submit and Flush because
    // the flush happens before the next AnimationSystem tick mutates it.
    // Null = static draw (no palette upload).
    const glm::mat4*     BonePalette      = nullptr;
    uint32_t             BonePaletteCount = 0;
};

// ── Instanced draw request ─────────────────────────────────────
// A batch of instances sharing the same Material + VertexArray.
// The VA must contain a per-instance buffer (mat4 model matrix
// at locations 0-3) alongside the mesh's per-vertex attributes.
// RenderQueue issues one DrawIndexedInstanced per batch.
struct InstancedRenderable
{
    DM::Ref<Material>    Material;
    DM::Ref<VertexArray> VertexArray;  // mesh VB + instance VB
    uint32_t             InstanceCount = 0;
};

class DMGE_API RenderQueue
{
public:
    // Discard queued renderables without submitting (e.g. between scenes).
    void Clear();

    // Enqueue a material-bound draw.
    void Submit(const DM::Ref<Material>& material,
                const DM::Ref<VertexArray>& vertexArray,
                const glm::mat4& transform = glm::mat4(1.0f));
    // Enqueue a material-bound skinned draw: same as above plus a per-draw
    // bone palette uploaded to u_BoneMatrices (animation stage 1, OpenGL).
    // The palette pointer must stay valid until Flush (see Renderable).
    void Submit(const DM::Ref<Material>& material,
                const DM::Ref<VertexArray>& vertexArray,
                const glm::mat4& transform,
                const glm::mat4* bonePalette, uint32_t bonePaletteCount);
    // Enqueue a shader-only draw (legacy path, no material uniforms).
    void Submit(const DM::Ref<Shader>& shader,
                const DM::Ref<VertexArray>& vertexArray,
                const glm::mat4& transform = glm::mat4(1.0f));

    // Enqueue an instanced batch: one DrawIndexedInstanced for
    // instanceCount instances sharing the same material + VA.
    // The VA must contain a per-instance buffer (already filled).
    void SubmitInstanced(const DM::Ref<Material>& material,
                         const DM::Ref<VertexArray>& vertexArray,
                         uint32_t instanceCount);

    // Sort by material/shader, then submit every queued renderable:
    // same group binds state + view-projection only once. The queue is
    // drained (cleared) after submission so it is ready for the next frame.
    void Flush(const glm::mat4& viewProjection,
                  const glm::vec3& cameraPosition,
                  const SceneLightData& lightData);

    // Deferred-path flush (3e stage 1, documents/DEFERRED_RENDERING_DESIGN.md):
    // submits every queued renderable into the G-buffer instead of the
    // forward shaders - per-draw and skinned draws go through the static /
    // skinned G-buffer shader, instanced batches through the instanced one.
    // Material values are copied per group from the queued Material onto the
    // G-buffer shader by well-known Blinn-Phong uniform names (2b debt: no
    // reflection). Light uniforms are NOT uploaded here (the G-buffer pass
    // stores geometry only; the lighting pass uploads them in Renderer).
    // The queue is drained (cleared) after submission.
    void FlushDeferred(const glm::mat4& viewProjection,
                       const DeferredShaderSet& shaders);

    // Number of renderables currently queued (read before Flush).
    std::size_t GetCount() const { return m_Queue.size(); }

private:
    std::vector<Renderable>         m_Queue;
};

} // namespace DMGameEngine
