/*
 * DMGameEngine - Scene implementation (ECS stage 1b)
 */
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/IDComponent.h"
#include "DMGameEngine/Scene/Components/TagComponent.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"

#include <random>

namespace DMGameEngine {

namespace {
    // Learning-grade 64-bit random UUID. Replace with a proper UUID lib for
    // production serialization (stage 1c).
    uint64_t GenerateUUID()
    {
        static std::mt19937_64 rng{std::random_device{}()};
        return rng();
    }
} // namespace

Entity Scene::CreateEntity(const std::string& tag)
{
    entt::entity e = m_Registry.create();
    m_Registry.emplace<IDComponent>(e, GenerateUUID());
    m_Registry.emplace<TagComponent>(e, tag);
    m_Registry.emplace<TransformComponent>(e);
    return FromEntt(e);
}

void Scene::DestroyEntity(Entity e)
{
    const entt::entity ent = ToEntt(e);
    auto* tc = m_Registry.try_get<TransformComponent>(ent);
    if (tc)
    {
        // Detach from parent's sibling chain.
        DetachFromParent(e);

        // Orphan children: each becomes an independent root (Parent=Null,
        // NextSibling=Null). They are NOT re-parented to the grandparent -
        // keeping the chain consistent with minimal logic. Re-parenting is
        // the caller's responsibility if a different policy is wanted.
        for (Entity c = tc->FirstChild; c != NullEntity; )
        {
            auto& ctc = m_Registry.get<TransformComponent>(ToEntt(c));
            const Entity next = ctc.NextSibling;  // save before clearing
            ctc.Parent = NullEntity;
            ctc.NextSibling = NullEntity;
            ctc.Dirty = true;
            c = next;
        }
    }
    m_Registry.destroy(ent);
}

void Scene::AddSystem(const DM::Ref<System>& sys)
{
    m_Systems.push_back(sys);
}

void Scene::OnUpdate(Timestep ts)
{
    for (auto& sys : m_Systems)
        sys->OnUpdate(ts);
}

void Scene::OnRender()
{
    for (auto& sys : m_Systems)
        sys->OnRender();
}

// ── Transform hierarchy ──────────────────────────────────────

void Scene::SetTranslation(Entity e, glm::vec3 t)
{
    auto& tc = m_Registry.get<TransformComponent>(ToEntt(e));
    tc.Translation = t;
    tc.Dirty = true;
    MarkSubtreeDirty(e);
}

void Scene::SetRotation(Entity e, glm::vec3 r)
{
    auto& tc = m_Registry.get<TransformComponent>(ToEntt(e));
    tc.RotationEuler = r;
    tc.Dirty = true;
    MarkSubtreeDirty(e);
}

void Scene::SetScale(Entity e, glm::vec3 s)
{
    auto& tc = m_Registry.get<TransformComponent>(ToEntt(e));
    tc.Scale = s;
    tc.Dirty = true;
    MarkSubtreeDirty(e);
}

void Scene::SetParent(Entity child, Entity newParent)
{
    // Reject cycles: newParent cannot be child or a descendant of child
    // (would form a loop in the Parent chain -> infinite recursion in
    // TransformSystem's topological walk).
    if (newParent != NullEntity &&
        (newParent == child || IsDescendant(child, newParent)))
    {
        return;  // invalid: would create a cycle
    }

    DetachFromParent(child);

    auto& ctc = m_Registry.get<TransformComponent>(ToEntt(child));
    if (newParent != NullEntity)
    {
        auto& np = m_Registry.get<TransformComponent>(ToEntt(newParent));
        ctc.Parent = newParent;
        ctc.NextSibling = np.FirstChild;   // insert at head of parent's child chain
        np.FirstChild = child;
    }
    else
    {
        ctc.Parent = NullEntity;
        ctc.NextSibling = NullEntity;
    }
    ctc.Dirty = true;
    MarkSubtreeDirty(child);
}

void Scene::MarkSubtreeDirty(Entity root)
{
    auto* tc = m_Registry.try_get<TransformComponent>(ToEntt(root));
    if (!tc) return;
    for (Entity c = tc->FirstChild; c != NullEntity; )
    {
        auto& ctc = m_Registry.get<TransformComponent>(ToEntt(c));
        const Entity next = ctc.NextSibling;  // save before recursion
        if (!ctc.Dirty)                        // prune: already dirty -> subtree already flagged
        {
            ctc.Dirty = true;
            MarkSubtreeDirty(c);
        }
        c = next;
    }
}

bool Scene::IsDescendant(Entity ancestor, Entity candidate) const
{
    // Walk candidate's Parent chain; if we hit 'ancestor', candidate is a descendant.
    const TransformComponent* tc = m_Registry.try_get<TransformComponent>(ToEntt(candidate));
    while (tc && tc->Parent != NullEntity)
    {
        if (tc->Parent == ancestor)
            return true;
        tc = m_Registry.try_get<TransformComponent>(ToEntt(tc->Parent));
    }
    return false;
}

void Scene::DetachFromParent(Entity child)
{
    auto& ctc = m_Registry.get<TransformComponent>(ToEntt(child));
    if (ctc.Parent == NullEntity)
        return;
    auto& parent = m_Registry.get<TransformComponent>(ToEntt(ctc.Parent));
    if (parent.FirstChild == child)
    {
        parent.FirstChild = ctc.NextSibling;
    }
    else
    {
        for (Entity prev = parent.FirstChild; prev != NullEntity; )
        {
            auto& prevTc = m_Registry.get<TransformComponent>(ToEntt(prev));
            if (prevTc.NextSibling == child)
            {
                prevTc.NextSibling = ctc.NextSibling;
                break;
            }
            prev = prevTc.NextSibling;
        }
    }
    ctc.Parent = NullEntity;
    ctc.NextSibling = NullEntity;
}

} // namespace DMGameEngine