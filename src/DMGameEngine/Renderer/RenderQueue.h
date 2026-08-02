/*
 * DMGameEngine - Render Queue (Deferred Submission + State Sorting)
 *
 * Collects draw requests throughout a frame and submits them in a single
 * sorted pass at Flush(). Renderables sharing the same Material (or, for
 * the legacy path, the same Shader) are grouped together so the shader +
 * material uniforms bind once per group, and the per-frame
 * view-projection is uploaded once per shader rather than once per draw.
 *
 * Each draw still issues a separate DrawIndexed (no geometry merging or
 * instancing) -- this is the first tier of batching: it removes redundant
 * state binds and uniform uploads, leaving the draw-call count unchanged
 * but cutting per-draw CPU/GPU state churn.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
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
    // Enqueue a shader-only draw (legacy path, no material uniforms).
    void Submit(const DM::Ref<Shader>& shader,
                const DM::Ref<VertexArray>& vertexArray,
                const glm::mat4& transform = glm::mat4(1.0f));

    // Sort by material/shader, then submit every queued renderable:
    // same group binds state + view-projection only once. The queue is
    // drained (cleared) after submission so it is ready for the next frame.
    void Flush(const glm::mat4& viewProjection,
                  const glm::vec3& cameraPosition,
                  const SceneLightData& lightData);

    // Number of renderables currently queued (read before Flush).
    std::size_t GetCount() const { return m_Queue.size(); }

private:
    std::vector<Renderable> m_Queue;
};

} // namespace DMGameEngine