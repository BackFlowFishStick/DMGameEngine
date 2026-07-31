/*
 * DMGameEngine - MeshRenderSystem (ECS stage 1b + 1c Mesh)
 *
 * Submits draws for entities with TransformComponent + MeshComponent. Uses the
 * Mesh's lazy-uploaded VertexArray; for the single-submesh case (the common
 * one) it submits the whole mesh with the first SubMesh's material. Multi-
 * submesh per-draw ranges need a Renderer::Submit range overload (follow-up).
 */
#pragma once
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Asset/AssetManager.h"

namespace DMGameEngine {

class MeshRenderSystem : public System
{
public:
    using System::System;

    void OnRender() override
    {
        auto& reg  = m_Scene.GetRegistry();
        auto  view = reg.view<TransformComponent, MeshComponent>();
        for (auto e : view)
        {
            auto [tc, mc] = view.get<TransformComponent, MeshComponent>(e);
            if (!mc.Mesh) continue;
            const auto& va = mc.Mesh->GetVertexArray();
            if (!va || mc.Mesh->SubMeshes.empty()) continue;

            // Per-instance override (MaterialOverrides[0]) wins; else the Mesh resource's
            // default material from SubMeshes[0].MaterialAsset. MaterialInstance is-a Material.
            DM::Ref<Material> material;
            if (!mc.MaterialOverrides.empty() && mc.MaterialOverrides[0])
                material = mc.MaterialOverrides[0];
            else
                material = AssetManager::Get().Load<Material>(mc.Mesh->SubMeshes[0].MaterialAsset);
            if (material)
                Renderer::Submit(material, va, tc.WorldMatrix);
        }
    }
};

} // namespace DMGameEngine