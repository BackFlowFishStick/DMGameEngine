/*
 * DMGameEngine - MeshRenderSystem (ECS stage 1b + 1c Mesh + Instancing)
 *
 * Dual-path rendering:
 *   - Groups entities by (Material, Mesh). Groups with >= kInstancingThreshold
 *     instances use DrawIndexedInstanced (1 draw call per group). The
 *     instance buffer (per-instance mat4 model matrix at locations 0-3)
 *     is packed from WorldMatrix data each frame.
 *   - Smaller groups, or those with per-instance MaterialInstance overrides,
 *     fall back to the per-draw path (DrawIndexed, existing RenderQueue path).
 *
 * Instanced VA caching: each unique Mesh gets a cached instanced VertexArray
 * that contains an instance buffer (dynamic, per-instance) + the mesh's own
 * vertex/index buffers (static). The instance buffer is updated via SetData
 * each frame; the VA persists across frames.
 *
 * Shader contract: instanced draws require a shader with per-instance
 * attributes (a_InstanceModel at locations 0-3, mesh attributes at 4+).
 * Non-instanced draws use u_Transform uniform. The game code is responsible
 * for using the correct shader variant per material.
 */
#pragma once
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/IndexBuffer.h"
#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Asset/Mesh.h"
#ifdef DMGE_ANIMATION
#include "DMGameEngine/Scene/Components/AnimatorComponent.h"
#endif

#include <unordered_map>
#include <vector>

namespace DMGameEngine {

class MeshRenderSystem : public System
{
public:
    explicit MeshRenderSystem(Scene& scene) : System(scene) {}

    const char* GetName() const override { return "MeshRenderSystem"; }

    void OnRender() override
    {
        auto& reg  = m_Scene.GetRegistry();
#ifdef DMGE_ANIMATION
        // Animated entities are drawn by SkinnedMeshRenderSystem with a bone
        // palette - exclude them here so a mesh is never double-drawn.
        auto  view = reg.view<TransformComponent, MeshComponent>(
                         entt::exclude<AnimatorComponent>);
#else
        auto  view = reg.view<TransformComponent, MeshComponent>();
#endif

        // ── Phase 1: group entities by (Material, Mesh) ──────────
        struct GroupKey
        {
            Material* mat;
            Mesh*     mesh;
            bool operator==(const GroupKey& o) const
            { return mat == o.mat && mesh == o.mesh; }
        };
        struct GroupKeyHash
        {
            size_t operator()(const GroupKey& k) const
            {
                return reinterpret_cast<size_t>(k.mat)
                     ^ (reinterpret_cast<size_t>(k.mesh) << 1);
            }
        };

        struct GroupData
        {
            DM::Ref<Material>    Material;
            DM::Ref<Mesh>        Mesh;
            DM::Ref<VertexArray> OriginalVA;
            std::vector<glm::mat4> Transforms;
        };

        std::unordered_map<GroupKey, GroupData, GroupKeyHash> groups;

        for (auto e : view)
        {
            auto [tc, mc] = view.get<TransformComponent, MeshComponent>(e);
            if (!mc.Mesh) continue;
            const auto& va = mc.Mesh->GetVertexArray();
            if (!va || mc.Mesh->SubMeshes.empty()) continue;

            DM::Ref<Material> material;
            if (!mc.MaterialOverrides.empty() && mc.MaterialOverrides[0])
                material = mc.MaterialOverrides[0];
            else
                material = AssetManager::Get().Load<Material>(
                    mc.Mesh->SubMeshes[0].MaterialAsset);
            if (!material) continue;

            GroupKey key{material.get(), mc.Mesh.get()};
            auto& g = groups[key];
            g.Material   = material;
            g.Mesh       = mc.Mesh;
            g.OriginalVA = va;
            g.Transforms.push_back(tc.WorldMatrix);
        }

        // ── Phase 2: render each group ───────────────────────────
        for (auto& [key, g] : groups)
        {
            if (g.Transforms.size() >= kInstancingThreshold)
            {
                // Instanced path: pack transforms into instance buffer,
                // submit as one (or few) DrawIndexedInstanced.
                const auto& instVA = GetOrCreateInstancedVA(g.Mesh);

                uint32_t total = static_cast<uint32_t>(g.Transforms.size());
                uint32_t offset = 0;
                while (offset < total)
                {
                    uint32_t batch = std::min(kMaxInstances, total - offset);
                    UpdateInstanceBuffer(instVA, g.Transforms.data() + offset, batch);
                    Renderer::SubmitInstanced(g.Material, instVA, batch);
                    offset += batch;
                }
            }
            else
            {
                // Per-draw path (existing): one Submit per entity.
                for (const auto& transform : g.Transforms)
                    Renderer::Submit(g.Material, g.OriginalVA, transform);
            }
        }
    }

private:
    static constexpr size_t  kInstancingThreshold = 8;
    static constexpr uint32_t kMaxInstances       = 4096;

    // ── Instanced VA cache ──────────────────────────────────────
    struct InstancedVAData
    {
        DM::Ref<VertexArray> VA;
        DM::Ref<VertexBuffer> InstanceVB;
    };
    std::unordered_map<Mesh*, InstancedVAData> m_InstancedVAs;

    const DM::Ref<VertexArray>& GetOrCreateInstancedVA(const DM::Ref<Mesh>& mesh)
    {
        auto it = m_InstancedVAs.find(mesh.get());
        if (it != m_InstancedVAs.end())
            return it->second.VA;

        InstancedVAData data;
        data.VA = VertexArray::Create();

        // Instance buffer FIRST -> locations 0-3 (mat4 = 4 columns).
        data.InstanceVB = VertexBuffer::Create(
            kMaxInstances * static_cast<uint32_t>(sizeof(glm::mat4)));
        data.InstanceVB->SetLayout({
            {ShaderDataType::Mat4, "a_InstanceModel", false, true},
        });
        data.VA->AddVertexBuffer(data.InstanceVB);

        // Mesh vertex buffer SECOND -> locations 4+.
        auto meshVB = VertexBuffer::Create(
            mesh->Vertices.data(),
            static_cast<uint32_t>(mesh->Vertices.size() * sizeof(float)));
        meshVB->SetLayout(mesh->Layout);
        data.VA->AddVertexBuffer(meshVB);

        // Index buffer.
        auto ib = IndexBuffer::Create(
            mesh->Indices.data(),
            static_cast<uint32_t>(mesh->Indices.size()));
        data.VA->SetIndexBuffer(ib);

        auto [inserted, _] = m_InstancedVAs.emplace(mesh.get(), std::move(data));
        return inserted->second.VA;
    }

    void UpdateInstanceBuffer(const DM::Ref<VertexArray>& va,
                              const glm::mat4* transforms,
                              uint32_t count)
    {
        for (auto& [mesh, data] : m_InstancedVAs)
        {
            if (data.VA == va)
            {
                data.InstanceVB->SetData(
                    transforms,
                    count * static_cast<uint32_t>(sizeof(glm::mat4)));
                return;
            }
        }
        DMGE_CORE_ASSERT(false, "UpdateInstanceBuffer: VA not found in cache!");
    }
};

} // namespace DMGameEngine
