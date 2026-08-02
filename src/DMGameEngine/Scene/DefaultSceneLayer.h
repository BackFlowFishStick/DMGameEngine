/*
 * DMGameEngine - Default Scene Layer
 *
 * A Layer that owns a CameraController and (optionally) a Scene, and
 * brackets its own render pass with Renderer::BeginScene(camera) /
 * EndScene(). Recommended base for gameplay / scene rendering.
 *
 * ECS integration (stage 1b): when a Scene is set via SetScene(), the
 * layer ticks Scene::OnUpdate each frame and, by default, calls
 * Scene::OnRender() inside the render bracket (running all registered
 * Systems' OnRender - i.e. MeshRenderSystem submits draws). The bracket
 * (BeginScene/EndScene) stays here so the camera - still CameraController -
 * owns the view-projection. See ECS_DESIGN.md section 10, plan A.
 * Override OnSceneRender() to add custom draws around the scene's output.
 *
 * The framebuffer is cleared once per frame by Application
 * (Renderer::ClearFrame); BeginScene/EndScene only cache the
 * view-projection and flush the per-pass render queue.
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
#include "DMGameEngine/Scene/Scene.h"

namespace DMGameEngine {

class DefaultSceneLayer : public Layer
{
public:
    explicit DefaultSceneLayer(const std::string& name = "DefaultScene")
        : Layer(name, LayerType::Feature) {}

    // ── Camera controller ───────────────────────────────────────
    // Owns its camera and drives it from input. If null, the scene
    // renders with an identity view-projection.
    void SetCameraController(const DM::Ref<CameraController>& controller)
    {
        m_CameraController = controller;
    }
    CameraController* GetCameraController() const { return m_CameraController.get(); }

    // ── Scene (ECS) ─────────────────────────────────────────────
    // When set, the layer ticks the scene each frame and renders its
    // Systems inside the camera's render bracket.
    void SetScene(const DM::Ref<Scene>& scene) { m_Scene = scene; }
    Scene* GetScene() const { return m_Scene.get(); }

    // ── Layer overrides ────────────────────────────────────────
    void OnUpdate(Timestep ts) override
    {
        if (m_CameraController)
            m_CameraController->OnUpdate(ts);
        if (m_Scene)
            m_Scene->OnUpdate(ts);   // tick Systems (TransformSystem computes world matrices)
    }

    void OnEvent(Event& event) override
    {
        if (m_CameraController)
            m_CameraController->OnEvent(event);
    }

    void OnRender() override
    {
        // Bracket this layer's render pass: cache the camera view-projection
        // (identity if no controller), then flush the per-pass queue after
        // the scene submits its draws.
        if (m_CameraController)
            Renderer::BeginScene(m_CameraController->GetCamera());
        else
            Renderer::BeginScene();

        OnSceneRender();

        Renderer::EndScene();
    }

protected:
    // Override to submit custom draws around the scene's system output.
    // Default: render the Scene's Systems (MeshRenderSystem submits draws).
    virtual void OnSceneRender()
    {
        if (m_Scene)
            m_Scene->OnRender();
    }

    DM::Ref<CameraController> m_CameraController;
    DM::Ref<Scene>           m_Scene;
};

} // namespace DMGameEngine
