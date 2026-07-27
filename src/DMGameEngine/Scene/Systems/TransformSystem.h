/*
 * DMGameEngine - TransformSystem (ECS stage 1b)
 *
 * Recomputes LocalMatrix + WorldMatrix for every TransformComponent, in
 * topological order (parent before child) so a parent's WorldMatrix is
 * ready when its children read it.
 *
 * Dirty-flag short-circuit: if a node is not Dirty, its whole subtree is
 * skipped (MarkSubtreeDirty guarantees children are also clean when a
 * parent is clean). Contract: mutate TransformComponent only through
 * Scene::SetTranslation/SetRotation/SetScale (or manually set Dirty=true
 * + call MarkSubtreeDirty); otherwise changes will be ignored here.
 *
 * See ECS_DESIGN.md section 7.2 and ECS_CONCEPTS.md part 2 (dirty
 * propagation) for the design.
 */
#pragma once
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

namespace DMGameEngine {

class TransformSystem : public System
{
public:
    explicit TransformSystem(Scene& scene) : System(scene) {}

    void OnUpdate(Timestep /*ts*/) override
    {
        auto& reg = m_Scene.GetRegistry();
        // Walk every root (Parent == NullEntity) depth-first. Children are
        // reached via the FirstChild/NextSibling chain, so this covers the
        // whole forest without iterating non-root entities twice.
        auto view = reg.view<TransformComponent>();
        for (auto e : view)
        {
            auto& tc = view.get<TransformComponent>(e);
            if (tc.Parent == NullEntity)
            {
                UpdateSubtree(reg, e, glm::mat4(1.0f));
            }
        }
    }

private:
    static void UpdateSubtree(entt::registry& reg, entt::entity e,
                              const glm::mat4& parentWorld)
    {
        auto& tc = reg.get<TransformComponent>(e);
        if (!tc.Dirty)
            return;  // subtree unchanged - propagation guarantees children are clean too

        tc.LocalMatrix = glm::translate(glm::mat4(1.0f), tc.Translation)
                       * glm::mat4_cast(glm::quat(tc.RotationEuler))
                       * glm::scale(glm::mat4(1.0f), tc.Scale);
        tc.WorldMatrix = parentWorld * tc.LocalMatrix;
        tc.Dirty = false;

        for (Entity c = tc.FirstChild; c != NullEntity; )
        {
            auto& ctc = reg.get<TransformComponent>(static_cast<entt::entity>(c));
            const Entity next = ctc.NextSibling;  // save before recursion
            UpdateSubtree(reg, static_cast<entt::entity>(c), tc.WorldMatrix);
            c = next;
        }
    }
};

} // namespace DMGameEngine