/*
 * DMGameEngine - Orthographic Camera Controller (2D)
 *
 * Concrete CameraController driving an OrthographicCamera. Provides
 * classic 2D scene navigation:
 *
 *   - Pan    : W / A / S / D or arrow keys translate the camera.
 *   - Roll   : Q / E rotate the camera about the view Z axis
 *              (opt-in via the `rotation` constructor flag).
 *   - Zoom   : mouse wheel scales the orthographic bounds around the
 *              camera position (zoom level 1 = default extent).
 *   - Resize : WindowResizeEvent keeps the projection aspect ratio in
 *              sync with the viewport.
 *
 * Translation speed scales with the current zoom level so panning feels
 * consistent across zoom ranges. Header-only, like the Camera family.
 */

#pragma once

#include "DMGameEngine/Renderer/CameraController.h"
#include "DMGameEngine/Renderer/OrthographicCamera.h"
#include "DMGameEngine/Core/Input.h"
#include "DMGameEngine/Core/KeyCodes.h"
#include "DMGameEngine/Core/MouseCodes.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"
#include "DMGameEngine/Core/Events/ApplicationEvent.h"

#include "glm/glm.hpp"
#include <algorithm>

namespace DMGameEngine {

class OrthographicCameraController : public CameraController
{
public:
    // `aspectRatio` is width / height of the viewport; `rotation`
    // enables Q / E roll control (off by default for 2D scenes).
    explicit OrthographicCameraController(float aspectRatio, bool rotation = false)
        : m_AspectRatio(aspectRatio),
          m_RotationEnabled(rotation),
          // Default zoom level is 1.0, so the initial bounds are
          // [-aspect, aspect] x [-1, 1].
          m_Camera(-aspectRatio, aspectRatio, -1.0f, 1.0f)
    {
        m_Camera.SetPosition(m_CameraPosition);
    }

    void OnUpdate(Timestep ts) override
    {
        if (!m_Enabled)
            return;

        Input& input = Input::Get();
        const float move = m_ZoomLevel * m_TranslationSpeed * ts;

        if (input.IsKeyPressed(KeyCode::A) || input.IsKeyPressed(KeyCode::Left))
            m_CameraPosition.x -= move;
        if (input.IsKeyPressed(KeyCode::D) || input.IsKeyPressed(KeyCode::Right))
            m_CameraPosition.x += move;
        if (input.IsKeyPressed(KeyCode::W) || input.IsKeyPressed(KeyCode::Up))
            m_CameraPosition.y += move;
        if (input.IsKeyPressed(KeyCode::S) || input.IsKeyPressed(KeyCode::Down))
            m_CameraPosition.y -= move;
        const float dz = 0.15f;

        {
            auto leftX = input.GetGamepadAxis(0, GamepadAxis::LeftX);
            auto leftY = input.GetGamepadAxis(0, GamepadAxis::LeftY);

            m_CameraPosition.x += leftX * dz;
            m_CameraPosition.y += leftY * dz;
        }


        m_Camera.SetPosition(m_CameraPosition);

        if (m_RotationEnabled)
        {
            if (input.IsKeyPressed(KeyCode::Q))
                m_CameraRotation += m_RotationSpeed * ts;
            else if (input.IsKeyPressed(KeyCode::E))
                m_CameraRotation -= m_RotationSpeed * ts;

            if (m_CameraRotation > 180.0f)
                m_CameraRotation -= 360.0f;
            else if (m_CameraRotation <= -180.0f)
                m_CameraRotation += 360.0f;

            m_Camera.SetRotation(m_CameraRotation);
        }
    }

    void OnEvent(Event& event) override
    {
        if (!m_Enabled)
            return;

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseScrolledEvent>(
            [this](MouseScrolledEvent& e) { return OnMouseScrolled(e); });
        dispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& e) { return OnWindowResized(e); });
    }

    // Covariant return: exposes the concrete OrthographicCamera.
    OrthographicCamera& GetCamera() override { return m_Camera; }

    // ── Zoom ────────────────────────────────────────────────────
    //  `zoomLevel` scales the orthographic bounds (1 = default extent,
    //  <1 zooms in, >1 zooms out). Clamped to a positive minimum.
    float GetZoomLevel() const { return m_ZoomLevel; }
    void SetZoomLevel(float zoomLevel)
    {
        m_ZoomLevel = std::max(zoomLevel, m_MinZoomLevel);
        RecalculateProjection();
    }

    // ── Aspect ratio / viewport ─────────────────────────────────
    float GetAspectRatio() const { return m_AspectRatio; }
    void SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;
        RecalculateProjection();
    }
    void SetViewportSize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }

    // ── Tunables ────────────────────────────────────────────────
    void SetTranslationSpeed(float speed) { m_TranslationSpeed = speed; }
    void SetRotationSpeed(float speed)    { m_RotationSpeed = speed; }
    void SetZoomSpeed(float speed)        { m_ZoomSpeed = speed; }
    float GetTranslationSpeed() const { return m_TranslationSpeed; }
    float GetRotationSpeed()    const { return m_RotationSpeed; }
    float GetZoomSpeed()        const { return m_ZoomSpeed; }

private:
    bool OnMouseScrolled(MouseScrolledEvent& e)
    {
        m_ZoomLevel -= e.GetYOffset() * m_ZoomSpeed;
        m_ZoomLevel = std::max(m_ZoomLevel, m_MinZoomLevel);
        RecalculateProjection();
        return false;
    }

    bool OnWindowResized(WindowResizeEvent& e)
    {
        SetViewportSize(e.GetWidth(), e.GetHeight());
        return false;
    }

    void RecalculateProjection()
    {
        m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel,
                                -m_ZoomLevel, m_ZoomLevel);
    }

    OrthographicCamera m_Camera;

    float m_AspectRatio = 16.0f / 9.0f;

    glm::vec3 m_CameraPosition = { 0.0f, 0.0f, 0.0f };
    float m_CameraRotation = 0.0f; // roll about Z, in degrees

    float m_TranslationSpeed = 5.0f;  // world units / second (at zoom 1)
    float m_RotationSpeed    = 180.0f; // degrees / second
    float m_ZoomSpeed        = 0.25f;  // zoom units per scroll notch
    float m_MinZoomLevel     = 0.25f;

    float m_ZoomLevel = 1.0f;

    bool m_RotationEnabled = false;
};

} // namespace DMGameEngine
