/*
 * DMGameEngine - Default Scene Layer
 *
 * A Layer that owns a CameraController and brackets its own render
 * pass with Renderer::BeginScene(camera) / EndScene(). This is the
 * recommended base for gameplay / scene rendering: derive from it,
 * set a camera controller (SetCameraController), and override
 * OnSceneRender() to submit draw commands.
 *
 * Why this exists:
 *   Application no longer holds a global "active camera" - that baked
 *   a single-camera model into the frame orchestrator and coupled
 *   Core (Application) to Renderer (CameraController). Each scene
 *   layer now owns its camera and brackets its own render pass, so
 *   multiple scene layers can render multiple cameras in one frame
 *   (split-screen, minimap RTT, etc.) by each calling
 *   BeginScene(cam) / EndScene().
 *
 * The framebuffer is cleared once per frame by Application
 * (Renderer::ClearFrame); BeginScene / EndScene only cache the
 * view-projection and flush the per-pass render queue, so earlier
 * passes' output is preserved across multiple scene layers.
 *
 * Lifecycle hooks forwarded to the controller:
 *   - OnUpdate(Timestep): advances the camera from polled input.
 *   - OnEvent(Event):      routes discrete input (mouse / scroll /
 *                          resize) to the controller.
 *
 * If no controller is set, the scene renders with an identity
 * view-projection (Renderer::BeginScene()).
 *
 * Header-only, matching the Scene camera-controller family.
 */

#pragma once

#include "DMGameEngine/Core/Layer.h"
#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Renderer/CameraController.h"
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/Renderer/Renderer.h"

namespace DMGameEngine {

class DefaultSceneLayer : public Layer
{
public:
    explicit DefaultSceneLayer(const std::string& name = "DefaultScene")
        : Layer(name, LayerType::Feature) {}

    // ── Camera controller ───────────────────────────────────────
    //  The controller owns its camera and drives it from input. If null,
    //  the scene renders with an identity view-projection.
    void SetCameraController(const DM::Ref<CameraController>& controller)
    {
        m_CameraController = controller;
    }
    CameraController* GetCameraController() const { return m_CameraController.get(); }

    // ── Layer overrides ────────────────────────────────────────
    void OnUpdate(Timestep ts) override
    {
        if (m_CameraController)
            m_CameraController->OnUpdate(ts);
    }

    void OnEvent(Event& event) override
    {
        if (m_CameraController)
            m_CameraController->OnEvent(event);
    }

    void OnRender() override
    {
        // Bracket this layer's render pass: cache the camera
        // view-projection (identity if no controller), then flush the
        // per-pass render queue after the scene submits its draws.
        if (m_CameraController)
            Renderer::BeginScene(m_CameraController->GetCamera());
        else
            Renderer::BeginScene();

        OnSceneRender();

        Renderer::EndScene();
    }

protected:
    // Override to submit scene draw commands (Renderer::Submit) between
    // the BeginScene / EndScene bracket established by OnRender.
    virtual void OnSceneRender() {}

    DM::Ref<CameraController> m_CameraController;
};

} // namespace DMGameEngine