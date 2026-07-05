/*
 * DMGameEngine - Camera Base Class
 *
 * Base class for all camera types. Owns the projection matrix that
 * maps world / view space into clip space.
 *
 * Concrete camera types (orthographic, perspective, scene camera)
 * derive from this and compute the projection via SetProjection()
 * — e.g. glm::ortho / glm::perspective — then read it back through
 * GetProjection() when submitting view-projection uniforms to shaders.
 *
 * The view matrix and any per-frame recalculation are the
 * responsibility of the derived type; this base only stores the
 * projection so renderers stay camera-agnostic.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"

namespace DMGameEngine {

class DMGE_API Camera
{
public:
    Camera() = default;
    explicit Camera(const glm::mat4& projection)
        : m_Projection(projection) {}

    virtual ~Camera() = default;

    // ── Projection ───────────────────────────────────────────────
    //  The projection matrix (perspective / orthographic) transforming
    //  view space into clip space. Derived cameras recompute and push
    //  it here whenever their parameters change.
    const glm::mat4& GetProjection() const { return m_Projection; }
    void SetProjection(const glm::mat4& projection) { m_Projection = projection; }

protected:
    glm::mat4 m_Projection = glm::mat4(1.0f);
};

} // namespace DMGameEngine