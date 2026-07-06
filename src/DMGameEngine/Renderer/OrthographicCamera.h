/*
 * DMGameEngine - Orthographic Camera
 *
 * Concrete orthographic camera derived from Camera. Owns an
 * orthographic projection (left / right / bottom / top / near / far)
 * and a camera transform described by a 2D position and a roll angle
 * (rotation about the view Z axis), from which the view matrix is
 * derived as the inverse of the camera world transform.
 *
 * Ideal for 2D scenes and isometric / HUD rendering. All parameters
 * can be changed at runtime; mutating any of them recomputes the
 * affected matrix immediately through the inherited SetProjection() /
 * SetView() accessors.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Camera.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/matrix_inverse.hpp"

namespace DMGameEngine {

class DMGE_API OrthographicCamera : public Camera
{
public:
    OrthographicCamera(float left, float right, float bottom, float top,
                       float nearClip = -1.0f, float farClip = 1.0f)
    {
        SetProjection(left, right, bottom, top, nearClip, farClip);
        RecalculateViewMatrix();
    }

    // ── Projection ───────────────────────────────────────────────
    void SetProjection(float left, float right, float bottom, float top,
                       float nearClip = -1.0f, float farClip = 1.0f)
    {
        m_Projection = glm::ortho(left, right, bottom, top, nearClip, farClip);
        RecalculateViewProjection();
    }

    // ── Transform ────────────────────────────────────────────────
    //  Position is in world units; rotation is a roll about the view Z
    //  axis expressed in radians. Mutating either recomputes the view.
    void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }
    const glm::vec3& GetPosition() const { return m_Position; }

    void SetRotation(float rotation) { m_Rotation = rotation; RecalculateViewMatrix(); }
    float GetRotation() const { return m_Rotation; }

private:
    void RecalculateViewMatrix()
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position)
                            * glm::rotate(glm::mat4(1.0f), m_Rotation,
                                          glm::vec3(0.0f, 0.0f, 1.0f));
        SetView(glm::inverse(transform));
    }

    glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
    float     m_Rotation = 0.0f; // roll about Z, in radians
};

} // namespace DMGameEngine