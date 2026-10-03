/*
 * DMGameEngine - SkinnedMeshRenderSystem (animation stage 1)
 *
 * Per-draw skinning pass: renders entities that have (TransformComponent,
 * MeshComponent, AnimatorComponent) with a per-draw bone palette uploaded as
 * the u_BoneMatrices uniform array (max 128 joints, matching the shader).
 * MeshRenderSystem excludes these entities, so static + skinned paths never
 * double-draw; the static/instancing path is untouched.
 *
 * Material resolution mirrors MeshRenderSystem (per-instance override wins,
 * else the Mesh resource's SubMesh[0].MaterialAsset). Stage 1 keeps one draw
 * per entity (no skinned instancing, no multi-submesh batching - follow-up).
 *
 * The vertex shader expects a skinned vertex layout: a_BoneIndices (Float4,
 * cast to ivec4) + a_BoneWeights, which the assimp import appends for meshes
 * carrying bones. See BlinnPhongSkinned.glsl.
 *
 * Header-only, gated by DMGE_ANIMATION. Compiled/rendered on OpenGL only in
 * stage 1 (Vulkan skinned path is a documented follow-up, kb/KB-03).
 */
#pragma once
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"
#include "DMGameEngine/Scene/Components/AnimatorComponent.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Asset/Mesh.h"

#include <algorithm>

namespace DMGameEngine {

class SkinnedMeshRenderSystem : public System
{
public:
    explicit SkinnedMeshRenderSystem(Scene& scene) : System(scene) {}

    const char* GetName() const override { return "SkinnedMeshRenderSystem"; }

    // Must match u_BoneMatrices[] length in BlinnPhongSkinned.glsl.
    static constexpr uint32_t kMaxBones = 128;

    void OnRender() override
    {
        auto& reg  = m_Scene.GetRegistry();
        auto  view = reg.view<TransformComponent, MeshComponent, AnimatorComponent>();

        for (auto e : view)
        {
            auto [tc, mc, ac] = view.get<TransformComponent, MeshComponent,
                                         AnimatorComponent>(e);
            if (!mc.Mesh || ac.Palette.empty()) continue;

            const auto& va = mc.Mesh->GetVertexArray();
            if (!va || mc.Mesh->SubMeshes.empty()) continue;

            DM::Ref<Material> material;
            if (!mc.MaterialOverrides.empty() && mc.MaterialOverrides[0])
                material = mc.MaterialOverrides[0];
            else
                material = AssetManager::Get().Load<Material>(
                    mc.Mesh->SubMeshes[0].MaterialAsset);
            if (!material) continue;

            Renderer::SubmitSkinned(material, va, tc.WorldMatrix,
                                    ac.Palette.data(),
                                    std::min<uint32_t>(
                                        static_cast<uint32_t>(ac.Palette.size()),
                                        kMaxBones));
        }
    }
};

} // namespace DMGameEngine
