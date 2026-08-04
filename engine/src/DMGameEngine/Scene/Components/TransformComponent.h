/*
 * DMGameEngine - TransformComponent (ECS stage 1b)
 *
 * Local transform + hierarchy (left-child / right-sibling triple chain)
 * + runtime matrix cache + dirty flag. Pure data POD - no logic methods
 * (see ECS_DESIGN.md section 6.3 and ECS_CONCEPTS.md section 1.5.2).
 *
 * Mutation path:
 *   - Local change: Scene::SetTranslation/SetRotation/SetScale sets the
 *     field, flags Dirty=true, then MarkSubtreeDirty propagates the flag
 *     down the subtree (see ECS_CONCEPTS.md part 2 - dirty propagation).
 *   - Recompute:    TransformSystem::OnUpdate recomputes LocalMatrix from
 *     Translation/RotationEuler/Scale, then WorldMatrix = parent.WorldMatrix
 *     * LocalMatrix, in topological order (parent before child).
 *
 * Hierarchy is stored as Entity IDs (not pointers) so the component stays
 * fixed-size POD and the triple chain is cache-friendly to walk. Use
 * Scene::SetParent to re-link the chain consistently.
 *
 * Serialization stores only Translation/RotationEuler/Scale + the chain;
 * LocalMatrix/WorldMatrix/Dirty are runtime-only and recomputed on load.
 */
#pragma once
#include "DMGameEngine/Scene/Entity.h"
#include "glm/glm.hpp"

namespace DMGameEngine {

struct TransformComponent
{
    // ── Local transform (serialized) ────────────────────────────
    glm::vec3 Translation{0.0f};
    glm::vec3 RotationEuler{0.0f};   // radians, Euler angles; serialization-friendly
    glm::vec3 Scale{1.0f};

    // ── Hierarchy: left-child / right-sibling triple chain ──────
    Entity Parent      = NullEntity;
    Entity FirstChild  = NullEntity;
    Entity NextSibling = NullEntity;

    // ── Runtime cache (not serialized) ─────────────────────────
    glm::mat4 LocalMatrix{1.0f};
    glm::mat4 WorldMatrix{1.0f};
    bool Dirty = true;   // local or an ancestor changed; WorldMatrix is stale

    TransformComponent() = default;
    TransformComponent(const TransformComponent&) = default;
    TransformComponent& operator=(const TransformComponent&) = default;

    // Convenience ctor for a root-level object's initial placement.
    explicit TransformComponent(glm::vec3 translation)
        : Translation(translation) {}
};

} // namespace DMGameEngine