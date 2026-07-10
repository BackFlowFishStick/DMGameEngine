/*
 * DMGameEngine - Camera Base Class
 *
 * Base class for all camera types. Owns the projection and view
 * matrices (and the cached projection * view product) that together
 * map world space into clip space.
 *
 * Concrete camera types (OrthographicCamera, PerspectiveCamera,
 * SceneCamera) derive from this and compute the projection via
 * SetProjection() — e.g. glm::ortho / glm::perspective — and the view
 * matrix via SetView() — e.g. glm::lookAt or the inverse of the camera
 * world transform. Renderers read the combined GetViewProjection()
 * when submitting view-projection uniforms to shaders.
 *
 * The view matrix and any per-frame recalculation are the
 * responsibility of the derived type; this base only stores the
 * projection and view matrices (and keeps the combined product in
 * sync) so renderers stay camera-agnostic.
 */

#pragma once

#include "glm/glm.hpp"
#include <cstdint>

namespace DMGameEngine {

class Camera
{
public:
    Camera() = default;
    explicit Camera(const glm::mat4& projection)
        : m_Projection(projection)
    {
        RecalculateViewProjection();
    }

    virtual ~Camera() = default;

    // ── Viewport ──────────────────────────────────────────────────────
    //  Hook invoked by the host (e.g. Application::OnEvent on a
    //  WindowResizeEvent) so the camera can refresh an aspect-ratio
    //  dependent projection. The base implementation is a no-op;
    //  derived types whose projection depends on the aspect ratio
    //  (PerspectiveCamera, SceneCamera) override it.
    virtual void OnViewportResize(uint32_t width, uint32_t height)
    {
        (void)width;
        (void)height;
    }

    // ── Projection ───────────────────────────────────────────────
    //  The projection matrix (perspective / orthographic) transforming
    //  view space into clip space. Derived cameras recompute and push
    //  it here whenever their parameters change.
    const glm::mat4& GetProjection() const { return m_Projection; }
    void SetProjection(const glm::mat4& projection)
    {
        m_Projection = projection;
        RecalculateViewProjection();
    }

    // ── View ─────────────────────────────────────────────────────
    //  The view matrix transforming world space into view (camera)
    //  space. Derived cameras recompute and push it here whenever
    //  the camera transform (position / rotation / target) changes.
    const glm::mat4& GetView() const { return m_View; }
    void SetView(const glm::mat4& view)
    {
        m_View = view;
        RecalculateViewProjection();
    }

    //  Combined projection * view, ready to submit as a u_ViewProjection
    //  uniform. Recomputed automatically whenever either matrix changes.
    const glm::mat4& GetViewProjection() const { return m_ViewProjection; }

protected:
    // Recomputes the cached projection * view product. Column-major
    // (GLM default): clip = Projection * View * World.
    void RecalculateViewProjection()
    {
        m_ViewProjection = m_Projection * m_View;
    }

    glm::mat4 m_Projection     = glm::mat4(1.0f);
    glm::mat4 m_View           = glm::mat4(1.0f);
    glm::mat4 m_ViewProjection  = glm::mat4(1.0f);
};

} // namespace DMGameEngine