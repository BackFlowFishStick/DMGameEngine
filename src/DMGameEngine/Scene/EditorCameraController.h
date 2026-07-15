/*
 * DMGameEngine - Editor Camera Controller (3D)
 *
 * Concrete CameraController driving a PerspectiveCamera with a
 * Maya/Unity-style orbit interaction model centred on a target:
 *
 *   - Orbit : hold the left mouse button and drag to rotate the
 *             camera (yaw / pitch) around the target.
 *   - Pan   : hold the right (or middle) mouse button and drag to
 *             translate the target (and camera) in the view plane.
 *   - Zoom  : mouse wheel changes the orbit distance (dolly).
 *   - Dolly : W / S / A / D / Space / LeftShift move the target along
 *             the view forward / right / up axes (polled in OnUpdate).
 *   - Resize: WindowResizeEvent keeps the projection aspect ratio in
 *             sync with the viewport.
 *
 * Pitch is clamped to just under +/-90 degrees so the up vector never
 * flips. Pan and dolly speeds scale with the orbit distance so motion
 * feels consistent across zoom ranges. Header-only, like the Camera
 * family.
 */

#pragma once

#include "DMGameEngine/Renderer/CameraController.h"
#include "DMGameEngine/Renderer/PerspectiveCamera.h"
#include "DMGameEngine/Core/Input.h"
#include "DMGameEngine/Core/KeyCodes.h"
#include "DMGameEngine/Core/MouseCodes.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"
#include "DMGameEngine/Core/Events/ApplicationEvent.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <algorithm>

namespace DMGameEngine {

class EditorCameraController : public CameraController
{
public:
    EditorCameraController(float fov = 45.0f,
                           float aspectRatio = 16.0f / 9.0f,
                           float nearClip = 0.1f,
                           float farClip = 1000.0f)
        : m_Fov(fov),
          m_AspectRatio(aspectRatio),
          m_NearClip(nearClip),
          m_FarClip(farClip),
          m_Camera(fov, aspectRatio, nearClip, farClip)
    {
        UpdateCameraView();
    }

    void OnUpdate(Timestep ts) override
    {
        if (!m_Enabled)
            return;

        Input& input = Input::Get();
        const glm::vec3 forward = ComputeForward();
        const glm::vec3 right   = ComputeRight(forward);

        glm::vec3 move(0.0f);
        if (input.IsKeyPressed(KeyCode::W)) move += forward;
        if (input.IsKeyPressed(KeyCode::S)) move -= forward;
        if (input.IsKeyPressed(KeyCode::D)) move += right;
        if (input.IsKeyPressed(KeyCode::A)) move -= right;
        if (input.IsKeyPressed(KeyCode::Space))    move += m_WorldUp;
        if (input.IsKeyPressed(KeyCode::LeftShift)) move -= m_WorldUp;

        if (float len = glm::length(move); len > 0.0f)
        {
            // Scale by distance so navigation stays consistent across zoom.
            const float speed = m_MoveSpeed * ts * (m_Distance * 0.5f + 1.0f);
            m_Target += (move / len) * speed;
            UpdateCameraView();
        }
    }

    void OnEvent(Event& event) override
    {
        if (!m_Enabled)
            return;

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseButtonPressedEvent>(
            [this](MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
        dispatcher.Dispatch<MouseButtonReleasedEvent>(
            [this](MouseButtonReleasedEvent& e) { return OnMouseButtonReleased(e); });
        dispatcher.Dispatch<MouseMovedEvent>(
            [this](MouseMovedEvent& e) { return OnMouseMoved(e); });
        dispatcher.Dispatch<MouseScrolledEvent>(
            [this](MouseScrolledEvent& e) { return OnMouseScrolled(e); });
        dispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& e) { return OnWindowResized(e); });
    }

    // Covariant return: exposes the concrete PerspectiveCamera.
    PerspectiveCamera& GetCamera() override { return m_Camera; }

    // ── Orbit target / transform ────────────────────────────────
    const glm::vec3& GetTarget() const { return m_Target; }
    void SetTarget(const glm::vec3& target) { m_Target = target; UpdateCameraView(); }

    float GetDistance() const { return m_Distance; }
    void SetDistance(float distance)
    {
        m_Distance = std::clamp(distance, m_MinDistance, m_MaxDistance);
        UpdateCameraView();
    }

    float GetYaw() const { return m_Yaw; }
    void SetYaw(float yaw) { m_Yaw = yaw; UpdateCameraView(); }

    float GetPitch() const { return m_Pitch; }
    void SetPitch(float pitch)
    {
        m_Pitch = std::clamp(pitch, -m_MaxPitch, m_MaxPitch);
        UpdateCameraView();
    }

    glm::vec3 GetPosition() const { return m_Target + ComputeOffset(); }

    // ── Projection ──────────────────────────────────────────────
    float GetFov()         const { return m_Fov; }
    float GetNearClip()    const { return m_NearClip; }
    float GetFarClip()     const { return m_FarClip; }
    float GetAspectRatio() const { return m_AspectRatio; }

    void SetViewportSize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        m_Camera.SetAspectRatio(m_AspectRatio);
    }

    // ── Tunables ────────────────────────────────────────────────
    void SetMoveSpeed(float speed)   { m_MoveSpeed = speed; }
    void SetRotateSpeed(float speed) { m_RotateSpeed = speed; }
    void SetPanSpeed(float speed)    { m_PanSpeed = speed; }
    void SetZoomSpeed(float speed)   { m_ZoomSpeed = speed; }
    float GetMoveSpeed()    const { return m_MoveSpeed; }
    float GetRotateSpeed()  const { return m_RotateSpeed; }
    float GetPanSpeed()     const { return m_PanSpeed; }
    float GetZoomSpeed()    const { return m_ZoomSpeed; }

private:
    // ── Event handlers ──────────────────────────────────────────
    bool OnMouseButtonPressed(MouseButtonPressedEvent& e)
    {
        const MouseCode button = e.GetMouseButton();
        if (button == MouseCode::Left)
        {
            m_IsOrbiting = true;
            m_LastMouse = { Input::Get().GetMouseX(), Input::Get().GetMouseY() };
        }
        else if (button == MouseCode::Right || button == MouseCode::Middle)
        {
            m_IsPanning = true;
            m_LastMouse = { Input::Get().GetMouseX(), Input::Get().GetMouseY() };
        }
        return false;
    }

    bool OnMouseButtonReleased(MouseButtonReleasedEvent& e)
    {
        const MouseCode button = e.GetMouseButton();
        if (button == MouseCode::Left)
            m_IsOrbiting = false;
        else if (button == MouseCode::Right || button == MouseCode::Middle)
            m_IsPanning = false;
        return false;
    }

    bool OnMouseMoved(MouseMovedEvent& e)
    {
        const glm::vec2 current{ e.GetX(), e.GetY() };
        const glm::vec2 delta = current - m_LastMouse;
        m_LastMouse = current;

        if (m_IsOrbiting)
        {
            m_Yaw   -= delta.x * m_RotateSpeed;
            m_Pitch += delta.y * m_RotateSpeed;
            m_Pitch  = std::clamp(m_Pitch, -m_MaxPitch, m_MaxPitch);
            UpdateCameraView();
        }
        else if (m_IsPanning)
        {
            const glm::vec3 forward = ComputeForward();
            const glm::vec3 right   = ComputeRight(forward);
            const glm::vec3 up      = glm::normalize(glm::cross(right, forward));

            const float scale = m_PanSpeed * m_Distance;
            // Grab-the-scene feel: drag right pans the view right.
            m_Target -= right * delta.x * scale;
            m_Target += up    * delta.y * scale;
            UpdateCameraView();
        }
        return false;
    }

    bool OnMouseScrolled(MouseScrolledEvent& e)
    {
        m_Distance -= e.GetYOffset() * m_ZoomSpeed;
        m_Distance = std::clamp(m_Distance, m_MinDistance, m_MaxDistance);
        UpdateCameraView();
        return false;
    }

    bool OnWindowResized(WindowResizeEvent& e)
    {
        SetViewportSize(e.GetWidth(), e.GetHeight());
        return false;
    }

    // ── Orbit geometry ──────────────────────────────────────────
    //  The camera sits on a sphere of radius `m_Distance` around the
    //  target, placed by yaw (about Y) and pitch (about X).
    glm::vec3 ComputeOffset() const
    {
        const float cp = glm::cos(glm::radians(m_Pitch));
        return {
            m_Distance * cp * glm::sin(glm::radians(m_Yaw)),
            m_Distance * glm::sin(glm::radians(m_Pitch)),
            m_Distance * cp * glm::cos(glm::radians(m_Yaw))
        };
    }

    // Direction from camera to target.
    glm::vec3 ComputeForward() const
    {
        return glm::normalize(-ComputeOffset());
    }

    glm::vec3 ComputeRight(const glm::vec3& forward) const
    {
        return glm::normalize(glm::cross(forward, m_WorldUp));
    }

    void UpdateCameraView()
    {
        m_Camera.SetPosition(GetPosition());
        m_Camera.SetTarget(m_Target);
    }

    // ── State ───────────────────────────────────────────────────
    PerspectiveCamera m_Camera;

    glm::vec3 m_Target  = { 0.0f, 0.0f, 0.0f };
    glm::vec3 m_WorldUp = { 0.0f, 1.0f, 0.0f };

    float m_Yaw      = 0.0f;  // degrees, about Y
    float m_Pitch    = 0.0f;  // degrees, about X, clamped to +/-m_MaxPitch
    float m_Distance = 10.0f; // orbit radius

    float m_Fov         = 45.0f;
    float m_AspectRatio = 16.0f / 9.0f;
    float m_NearClip    = 0.1f;
    float m_FarClip     = 1000.0f;

    // ── Tunables ────────────────────────────────────────────────
    float m_MoveSpeed   = 5.0f;   // target units / second (at distance ~0)
    float m_RotateSpeed = 0.25f;  // degrees per pixel of mouse drag
    float m_PanSpeed    = 0.001f;
    float m_ZoomSpeed   = 1.0f;   // distance units per scroll notch

    float m_MinDistance = 0.1f;
    float m_MaxDistance = 1000.0f;
    float m_MaxPitch    = 89.0f; // just under 90 to avoid the up flip

    // ── Drag state ──────────────────────────────────────────────
    glm::vec2 m_LastMouse = { 0.0f, 0.0f };
    bool m_IsOrbiting = false;
    bool m_IsPanning  = false;
};

} // namespace DMGameEngine
