/*
 * DMGameEngine - Perspective Camera
 *
 * Concrete perspective camera derived from Camera. Owns a perspective
 * projection (vertical field of view / aspect ratio / near / far) and a
 * camera transform described by a position, a target (look-at point)
 * and an up vector, from which the view matrix is derived via
 * glm::lookAt.
 *
 * Ideal for 3D scenes. The aspect ratio is exposed so renderers can
 * keep it in sync with the viewport (SetAspectRatio() / the dedicated
 * setters on SceneCamera); mutating any parameter recomputes the
 * affected matrix immediately.
 */

#pragma once

#include "DMGameEngine/Renderer/Camera.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

namespace DMGameEngine {

class PerspectiveCamera : public Camera
{
public:
    PerspectiveCamera(float fov, float aspectRatio,
                      float nearClip = 0.1f, float farClip = 1000.0f)
    {
        SetProjection(fov, aspectRatio, nearClip, farClip);
        RecalculateViewMatrix();
    }

    // ── Projection ───────────────────────────────────────────────
    //  `fov` is the vertical field of view in degrees (converted to radians internally); `aspectRatio` is
    //  width / height.
    void SetProjection(float fov, float aspectRatio, float nearClip, float farClip)
    {
        m_Fov         = fov;
        m_AspectRatio = aspectRatio;
        m_NearClip    = nearClip;
        m_FarClip     = farClip;
        m_Projection  = glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
        RecalculateViewProjection();
    }

    // Aspect-ratio convenience: keeps FOV / near / far, only updates the
    // ratio (e.g. on WindowResizeEvent) and recomputes the projection.
    void SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;
        m_Projection  = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
        RecalculateViewProjection();
    }
    float GetAspectRatio() const { return m_AspectRatio; }

    float GetFov()      const { return m_Fov; }
    float GetNearClip() const { return m_NearClip; }
    float GetFarClip()  const { return m_FarClip; }

    // ── Transform ────────────────────────────────────────────────
    void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }
    const glm::vec3& GetPosition() const { return m_Position; }

    void SetTarget(const glm::vec3& target) { m_Target = target; RecalculateViewMatrix(); }
    const glm::vec3& GetTarget() const { return m_Target; }

    void SetUp(const glm::vec3& up) { m_Up = up; RecalculateViewMatrix(); }
    const glm::vec3& GetUp() const { return m_Up; }

private:
    void RecalculateViewMatrix()
    {
        SetView(glm::lookAt(m_Position, m_Target, m_Up));
    }

    glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 m_Target   = { 0.0f, 0.0f, -1.0f };
    glm::vec3 m_Up       = { 0.0f, 1.0f, 0.0f };

    float m_Fov         = 45.0f; // vertical FOV in degrees (converted to radians internally)
    float m_AspectRatio = 16.0f / 9.0f;
    float m_NearClip    = 0.1f;
    float m_FarClip     = 1000.0f;
};

} // namespace DMGameEngine