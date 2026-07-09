/*
 * DMGameEngine - Scene Camera
 *
 * Runtime-switchable camera derived from Camera. Maintains the
 * parameters for both an orthographic and a perspective projection
 * simultaneously and can flip between them at runtime via
 * SetProjectionType(). A single aspect ratio drives both projections
 * (SetViewportSize() / SetAspectRatio()), so a window / viewport
 * resize keeps every projection type correct without the caller
 * reconfiguring each one.
 *
 * The view matrix is not managed here: SceneCamera is intended to be
 * driven by a scene transform (e.g. a TransformComponent), so the view
 * is supplied externally through the inherited SetView() when needed.
 */

#pragma once

#include "DMGameEngine/Renderer/Camera.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <cstdint>

namespace DMGameEngine {

class SceneCamera : public Camera
{
public:
    enum class ProjectionType : uint8_t
    {
        Perspective = 0,
        Orthographic = 1
    };

    SceneCamera() { RecalculateProjection(); }
    ~SceneCamera() override = default;

    // ── Projection type ──────────────────────────────────────────
    ProjectionType GetProjectionType() const { return m_ProjectionType; }
    void SetProjectionType(ProjectionType type)
    {
        m_ProjectionType = type;
        RecalculateProjection();
    }

    // ── Perspective parameters ───────────────────────────────────
    //  Vertical FOV is in radians.
    float GetPerspectiveVerticalFOV() const { return m_PerspectiveFOV; }
    void  SetPerspectiveVerticalFOV(float verticalFOV)
    {
        m_PerspectiveFOV = verticalFOV;
        RecalculateProjection();
    }
    float GetPerspectiveNearClip() const { return m_PerspectiveNear; }
    void  SetPerspectiveNearClip(float nearClip)
    {
        m_PerspectiveNear = nearClip;
        RecalculateProjection();
    }
    float GetPerspectiveFarClip() const { return m_PerspectiveFar; }
    void  SetPerspectiveFarClip(float farClip)
    {
        m_PerspectiveFar = farClip;
        RecalculateProjection();
    }

    // ── Orthographic parameters ──────────────────────────────────
    //  `size` is the full height of the visible frustum (in world units);
    //  the width is derived from size * aspectRatio.
    float GetOrthographicSize() const { return m_OrthographicSize; }
    void  SetOrthographicSize(float size)
    {
        m_OrthographicSize = size;
        RecalculateProjection();
    }
    float GetOrthographicNearClip() const { return m_OrthographicNear; }
    void  SetOrthographicNearClip(float nearClip)
    {
        m_OrthographicNear = nearClip;
        RecalculateProjection();
    }
    float GetOrthographicFarClip() const { return m_OrthographicFar; }
    void  SetOrthographicFarClip(float farClip)
    {
        m_OrthographicFar = farClip;
        RecalculateProjection();
    }

    // ── Aspect ratio / viewport ──────────────────────────────────
    //  Drives both projection types; call on window / viewport resize.
    //  A zero / negative size is ignored to avoid producing a degenerate
    //  (NaN) projection.
    float GetAspectRatio() const { return m_AspectRatio; }
    void  SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;
        RecalculateProjection();
    }
    void SetViewportSize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        RecalculateProjection();
    }

private:
    void RecalculateProjection()
    {
        if (m_ProjectionType == ProjectionType::Perspective)
        {
            m_Projection = glm::perspective(m_PerspectiveFOV, m_AspectRatio,
                                             m_PerspectiveNear, m_PerspectiveFar);
        }
        else
        {
            const float orthoLeft   = -m_OrthographicSize * m_AspectRatio * 0.5f;
            const float orthoRight  =  m_OrthographicSize * m_AspectRatio * 0.5f;
            const float orthoBottom = -m_OrthographicSize * 0.5f;
            const float orthoTop    =  m_OrthographicSize * 0.5f;
            m_Projection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop,
                                      m_OrthographicNear, m_OrthographicFar);
        }
        RecalculateViewProjection();
    }

    ProjectionType m_ProjectionType = ProjectionType::Perspective;

    float m_PerspectiveFOV  = glm::radians(45.0f);
    float m_PerspectiveNear = 0.01f;
    float m_PerspectiveFar  = 1000.0f;

    float m_OrthographicSize = 10.0f;
    float m_OrthographicNear = 0.01f;
    float m_OrthographicFar  = 1000.0f;

    float m_AspectRatio = 16.0f / 9.0f;
};

} // namespace DMGameEngine