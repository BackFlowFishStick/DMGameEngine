/*
 * DMGameEngine - MeshComponent (ECS stage 1b)
 *
 * Renderable mesh: VertexArray (vertex/index buffers) + Material
 * (shader + uniform bundle). MeshRenderSystem calls
 *   Renderer::Submit(MeshComponent.Material, MeshComponent.VAO,
 *                     TransformComponent.WorldMatrix)
 * which matches the existing Renderer::Submit signature exactly - zero
 * adaptation needed (see ECS_DESIGN.md section 2 / 6.4).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"        // DM::Ref
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/Material.h"

namespace DMGameEngine {

struct MeshComponent
{
    DM::Ref<VertexArray> VAO;
    DM::Ref<Material>    Material;

    MeshComponent() = default;
    MeshComponent(const MeshComponent&) = default;
    MeshComponent& operator=(const MeshComponent&) = default;
};

} // namespace DMGameEngine