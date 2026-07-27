/*
 * DMGameEngine - CameraComponent (ECS stage 1b)
 *
 * Wraps SceneCamera + marks the active camera. DEFINED but NOT enabled in
 * stage 1b: cameras are still driven by DefaultSceneLayer's CameraController
 * (see ECS_DESIGN.md section 10, integration plan A). A later step lets
 * CameraComponent take over: Scene queries Primary==true to pick the active
 * camera and calls Renderer::BeginScene with it. FixedAspectRatio is
 * reserved for the editor (stage 2a) viewport camera.
 */
#pragma once
#include "DMGameEngine/Scene/SceneCamera.h"

namespace DMGameEngine {

struct CameraComponent
{
    SceneCamera Camera;
    bool Primary = false;            // marks the active camera (view queries this)
    bool FixedAspectRatio = false;  // editor viewport camera (stage 2a)

    CameraComponent() = default;
    CameraComponent(const CameraComponent&) = default;
    CameraComponent& operator=(const CameraComponent&) = default;
};

} // namespace DMGameEngine