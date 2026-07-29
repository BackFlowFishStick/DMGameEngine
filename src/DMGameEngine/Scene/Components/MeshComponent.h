/*
 * DMGameEngine - MeshComponent (ECS stage 1b + 1c)
 *
 * Renderable mesh: VertexArray (vertex/index buffers) + Material (shader +
 * uniform bundle). Stage 1c adds AssetHandle fields (meshAsset/materialAsset
 * UUID) so the resource references serialize as stable UUIDs; the Ref<VAO> /
 * Ref<Material> are runtime-loaded from those UUIDs via AssetManager
 * (AssetLoader<Mesh/Material> is stage 1c follow-up, so they stay null after
 * deserialization until that lands).
 *
 * MeshRenderSystem calls Renderer::Submit(Material, VAO, WorldMatrix) - when
 * VAO/Material are null (resource not yet loaded) the draw is skipped.
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Asset/AssetHandle.h"

namespace DMGameEngine {

struct MeshComponent
{
    AssetHandle MeshAsset;        // UUID of mesh resource (serialized)
    AssetHandle MaterialAsset;    // UUID of material resource (serialized)
    DM::Ref<VertexArray> VAO;     // runtime, loaded from MeshAsset (AssetLoader<Mesh> follow-up)
    DM::Ref<Material>    Material; // runtime, loaded from MaterialAsset (AssetLoader<Material> follow-up)

    MeshComponent() = default;
    MeshComponent(const MeshComponent&) = default;
    MeshComponent& operator=(const MeshComponent&) = default;
};

} // namespace DMGameEngine