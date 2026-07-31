/*
 * DMGameEngine - MeshComponent (ECS stage 1b + 1c Mesh)
 *
 * Holds a Mesh resource (UUID + runtime Ref<Mesh>). The Mesh owns its
 * VertexArray (lazy upload) and SubMeshes (each with a material AssetHandle),
 * so MeshComponent no longer carries VAO/Material/MaterialAsset directly -
 * materials live per-SubMesh inside the Mesh. MeshRenderSystem walks SubMeshes
 * to submit draws.
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Asset/Mesh.h"
#include "DMGameEngine/Renderer/Material.h"  // MaterialInstance for per-instance overrides
#include <vector>

namespace DMGameEngine {

struct MeshComponent
{
    AssetHandle   MeshAsset;   // Mesh resource UUID (serialized)
    DM::Ref<Mesh> Mesh;        // runtime, loaded from MeshAsset via AssetManager
    // Per-instance material overrides (one per SubMesh; null = use the Mesh resource's
    // default material from SubMesh.MaterialAsset). Set at runtime to tweak a specific
    // entity's materials without affecting other entities sharing the same Mesh.
    std::vector<DM::Ref<MaterialInstance>> MaterialOverrides;

    MeshComponent() = default;
    MeshComponent(const MeshComponent&) = default;
    MeshComponent& operator=(const MeshComponent&) = default;
};

} // namespace DMGameEngine