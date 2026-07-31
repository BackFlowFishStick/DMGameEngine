/*
 * DMGameEngine - Mesh (stage 1c+ : mesh resource intermediate)
 *
 * CPU-side mesh data loaded from a file (.mesh JSON now; FBX/OBJ/GLTF via
 * assimp later). Holds interleaved vertices + indices + a list of SubMeshes
 * (each a material group: index range + material AssetHandle) + the vertex
 * BufferLayout. GetVertexArray() lazily uploads to the GPU and caches the
 * VertexArray, decoupling file parsing from GPU upload.
 *
 * MeshComponent will hold Ref<Mesh>; MeshRenderSystem walks SubMeshes to
 * submit one draw per material group (single-submesh case = whole mesh with
 * one material). Multi-submesh draw ranges are a follow-up (Renderer::Submit
 * range overload or per-submesh VertexArray).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Shader.h"        // BufferLayout, BufferElement
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Asset/AssetHandle.h"

#include <vector>
#include <cstdint>

namespace DMGameEngine {

struct DMGE_API SubMesh
{
    uint32_t    IndexOffset = 0;   // offset into Mesh::Indices
    uint32_t    IndexCount  = 0;   // index count for this submesh
    AssetHandle MaterialAsset;     // material UUID for this submesh
};

class DMGE_API Mesh
{
public:
    std::vector<float>        Vertices;   // interleaved per Layout
    std::vector<uint32_t>     Indices;
    std::vector<SubMesh>      SubMeshes;  // material groups (default: one covering all indices)
    BufferLayout              Layout;

    // Lazily upload vertices/indices to a VertexArray on first call; cached.
    // Returns null if Vertices is empty.
    const DM::Ref<VertexArray>& GetVertexArray() const;

    uint32_t GetIndexCount() const { return static_cast<uint32_t>(Indices.size()); }

private:
    mutable DM::Ref<VertexArray> m_VertexArray;
};

} // namespace DMGameEngine