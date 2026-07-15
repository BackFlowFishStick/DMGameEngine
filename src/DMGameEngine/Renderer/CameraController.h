/*
 * DMGameEngine - Camera Controller (Base)
 *
 * Abstract base for camera controllers. A controller bridges the
 * engine input system to a Camera: it advances the camera transform
 * every frame from OnUpdate(Timestep) using polled keyboard/mouse
 * state (Input::Get()), and reacts to discrete input events routed
 * through OnEvent(Event&) (mouse move / scroll / button / key /
 * window resize).
 *
 * Concrete controllers derive from this and own the Camera they drive:
 *   - OrthographicCameraController : 2D pan / roll / zoom over an
 *      OrthographicCamera (Scene/OrthographicCameraController.h).
 *   - EditorCameraController : 3D orbit / pan / zoom over a
 *      PerspectiveCamera (Scene/EditorCameraController.h).
 *
 * Controllers are intentionally not Layers: they expose the same
 * OnUpdate() / OnEvent() surface so a Layer (or Scene / Application)
 * can own one and forward its per-frame delta and events. SetEnabled
 * (false) suspends input handling (e.g. while an ImGui panel captures
 * the pointer) while keeping the last camera transform intact.
 *
 * Header-only, matching the Camera family it targets.
 */

#pragma once

#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Renderer/Camera.h"

namespace DMGameEngine {

class CameraController
{
public:
    virtual ~CameraController() = default;

    // Per-frame update: advances the camera from polled input by `ts`
    // seconds. Called once per frame before rendering.
    virtual void OnUpdate(Timestep ts) = 0;

    // Routes a single input event to this controller (mouse move /
    // scroll / button press or release / key / window resize). The
    // event's Handled flag follows the engine EventDispatcher
    // convention; handlers return false so input keeps propagating to
    // other layers unless intentionally consumed.
    virtual void OnEvent(Event& event) = 0;

    // The camera this controller drives.
    virtual Camera& GetCamera() = 0;
    const Camera& GetCamera() const { return const_cast<CameraController*>(this)->GetCamera(); }

    // Gate input handling. When disabled, OnUpdate() / OnEvent()
    // ignore input but the camera keeps its last transform. Defaults
    // to true.
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

protected:
    CameraController() = default;
    bool m_Enabled = true;
};

} // namespace DMGameEngine
