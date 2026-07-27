/*
 * DMGameEngine - MeshRenderSystem (ECS stage 1b)
 *
 * Submits one draw per entity that has both TransformComponent and
 * MeshComponent, using the world matrix computed by TransformSystem.
 * Calls Renderer::Submit(Material, VertexArray, WorldMatrix) - the
 * signature matches exactly, so no adapter is needed (see ECS_DESIGN.md
 * section 2 / 7.3).
 *
 * Does NOT call BeginScene/EndScene - the render-pass bracket (camera
 * view-projection + queue flush) is owned by DefaultSceneLayer (plan A).
 */
#pragma once
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"
#include "DMGameEngine/Renderer/Renderer.h"

namespace DMGameEngine {

class MeshRenderSystem : public System
{
public:
    explicit MeshRenderSystem(Scene& scene) : System(scene) {}

    void OnRender() override
    {
        auto& reg = m_Scene.GetRegistry();
        auto view = reg.view<TransformComponent, MeshComponent>();
        for (auto e : view)
        {
            auto [tc, mc] = view.get<TransformComponent, MeshComponent>(e);
            if (mc.VAO && mc.Material)
                Renderer::Submit(mc.Material, mc.VAO, tc.WorldMatrix);
        }
    }
};

} // namespace DMGameEngine