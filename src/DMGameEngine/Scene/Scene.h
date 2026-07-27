/*
 * DMGameEngine - Scene (ECS stage 1b)
 *
 * Owns an entt::registry + a list of Systems. Entity = uint32_t (DMGE alias),
 * converted to/from entt::entity at the boundary. CreateEntity auto-attaches
 * IDComponent + TagComponent + TransformComponent so every entity has stable
 * identity, a name, and a transform by default.
 *
 * Frame lifecycle: OnUpdate ticks each System (TransformSystem computes world
 * matrices first), OnRender calls each System::OnRender (MeshRenderSystem
 * submits draws). OnRender does NOT call BeginScene/EndScene - the render-pass
 * bracket is owned by DefaultSceneLayer (see ECS_DESIGN.md section 10, plan A).
 *
 * Transform hierarchy: local setters flag Dirty + MarkSubtreeDirty propagates
 * the flag down the subtree; TransformSystem recomputes in topological order
 * (see ECS_CONCEPTS.md part 2 - dirty propagation).
 *
 * This is the first header to #include <entt/entt.hpp>, so compiling it
 * validates the entt integration end-to-end (FetchContent + EnTT::EnTT +
 * PUBLIC link, see ECS_DESIGN.md section 11/12).
 */
#pragma once
#include <entt/entt.hpp>
#include <string>
#include <vector>

#include "DMGameEngine/Core/Export.h"           // DMGE_API, DM::Ref
#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Systems/System.h"
#include "glm/glm.hpp"

namespace DMGameEngine {

class DMGE_API Scene
{
public:
    Scene() = default;
    ~Scene() = default;

    // Non-copyable: owns a registry + system list with back-references.
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // ── Entity lifecycle ──────────────────────────────────────
    // CreateEntity auto-attaches IDComponent + TagComponent + TransformComponent.
    Entity CreateEntity(const std::string& tag = "Entity");
    void   DestroyEntity(Entity e);

    // ── Component access (delegates to entt::registry) ─────────
    template<typename T, typename... Args>
    T& AddComponent(Entity e, Args&&... args)
    {
        return m_Registry.emplace<T>(ToEntt(e), std::forward<Args>(args)...);
    }
    template<typename T>
    T& GetComponent(Entity e) { return m_Registry.get<T>(ToEntt(e)); }
    template<typename T>
    bool HasComponent(Entity e) { return m_Registry.all_of<T>(ToEntt(e)); }
    template<typename T>
    void RemoveComponent(Entity e) { m_Registry.remove<T>(ToEntt(e)); }

    // ── Systems ───────────────────────────────────────────────
    // Registration order is execution order.
    void AddSystem(const DM::Ref<System>& sys);

    // ── Frame lifecycle ────────────────────────────────────────
    void OnUpdate(Timestep ts);
    void OnRender();

    // ── Transform hierarchy helpers ───────────────────────────
    // Each local setter sets the field, flags Dirty=true, then propagates
    // dirty down the subtree so descendants recompute WorldMatrix.
    void SetTranslation(Entity e, glm::vec3 t);
    void SetRotation(Entity e, glm::vec3 r);
    void SetScale(Entity e, glm::vec3 s);
    void SetParent(Entity child, Entity newParent);
    void MarkSubtreeDirty(Entity root);

    // ── Accessors ─────────────────────────────────────────────
    entt::registry&       GetRegistry()       { return m_Registry; }
    const entt::registry& GetRegistry() const { return m_Registry; }

private:
    // DMGE Entity (uint32_t) <-> entt::entity conversion at the boundary.
    static entt::entity ToEntt(Entity e)        { return static_cast<entt::entity>(e); }
    static Entity       FromEntt(entt::entity e) { return static_cast<Entity>(e); }

    // True if 'candidate' is a descendant of 'ancestor' (walks Parent chain).
    // Used by SetParent to reject cycles that would loop TransformSystem.
    bool IsDescendant(Entity ancestor, Entity candidate) const;

    // Detach 'child' from its current parent's sibling chain (no-op if root).
    void DetachFromParent(Entity child);

    entt::registry m_Registry;
    std::vector<DM::Ref<System>> m_Systems;
};

} // namespace DMGameEngine