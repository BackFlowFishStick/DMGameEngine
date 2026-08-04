/*
 * DMGameEngine - ECS unit tests (pure logic, no GPU)
 *
 * Validates the stage-1b ECS core: CreateEntity auto-attaches components,
 * TransformSystem computes world matrices in topological order, dirty
 * propagation flags descendants, SetParent maintains the triple-chain and
 * rejects cycles, DestroyEntity orphans children. Runs headless (Scene +
 * TransformSystem only; MeshRenderSystem needs a graphics context).
 */

#include <gtest/gtest.h>

#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Components/IDComponent.h"
#include "DMGameEngine/Scene/Components/TagComponent.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Systems/TransformSystem.h"

#include "glm/glm.hpp"

using namespace DMGameEngine;

// Helper: register a TransformSystem on a fresh scene.
static void WithTransformSystem(Scene& scene)
{
    scene.AddSystem(DM::CreateRef<TransformSystem>(scene));
}

// ── CreateEntity ─────────────────────────────────────────────────

TEST(SceneECS, CreateEntityAttachesDefaultComponents)
{
    Scene scene;
    Entity e = scene.CreateEntity("Player");
    EXPECT_NE(e, NullEntity);
    EXPECT_TRUE(scene.HasComponent<IDComponent>(e));
    EXPECT_TRUE(scene.HasComponent<TagComponent>(e));
    EXPECT_TRUE(scene.HasComponent<TransformComponent>(e));
    EXPECT_EQ(scene.GetComponent<TagComponent>(e).Tag, std::string("Player"));

    auto& tc = scene.GetComponent<TransformComponent>(e);
    EXPECT_FLOAT_EQ(tc.Translation.x, 0.0f);
    EXPECT_FLOAT_EQ(tc.Scale.x, 1.0f);
    EXPECT_TRUE(tc.Dirty);
    EXPECT_EQ(tc.Parent, NullEntity);
}

// ── TransformSystem: root world matrix ───────────────────────────

TEST(TransformSystemTest, RootWorldMatrixFromTranslation)
{
    Scene scene;
    WithTransformSystem(scene);
    Entity e = scene.CreateEntity();
    scene.SetTranslation(e, glm::vec3(1.0f, 2.0f, 3.0f));
    scene.OnUpdate(Timestep(0.0f));

    auto& tc = scene.GetComponent<TransformComponent>(e);
    EXPECT_NEAR(tc.WorldMatrix[3].x, 1.0f, 1e-5f);
    EXPECT_NEAR(tc.WorldMatrix[3].y, 2.0f, 1e-5f);
    EXPECT_NEAR(tc.WorldMatrix[3].z, 3.0f, 1e-5f);
    EXPECT_FALSE(tc.Dirty);
}

TEST(TransformSystemTest, ScaleAffectsWorldMatrixDiagonal)
{
    Scene scene;
    WithTransformSystem(scene);
    Entity e = scene.CreateEntity();
    scene.SetScale(e, glm::vec3(2.0f, 3.0f, 4.0f));
    scene.OnUpdate(Timestep(0.0f));

    auto& tc = scene.GetComponent<TransformComponent>(e);
    EXPECT_NEAR(tc.WorldMatrix[0].x, 2.0f, 1e-5f);
    EXPECT_NEAR(tc.WorldMatrix[1].y, 3.0f, 1e-5f);
    EXPECT_NEAR(tc.WorldMatrix[2].z, 4.0f, 1e-5f);
}

// ── Parent / child hierarchy ──────────────────────────────────────

TEST(TransformSystemTest, ChildWorldMatrixInheritsParent)
{
    Scene scene;
    WithTransformSystem(scene);
    Entity parent = scene.CreateEntity("Parent");
    Entity child  = scene.CreateEntity("Child");
    scene.SetParent(child, parent);
    scene.SetTranslation(parent, glm::vec3(5.0f, 0.0f, 0.0f));
    scene.SetTranslation(child,  glm::vec3(1.0f, 0.0f, 0.0f));
    scene.OnUpdate(Timestep(0.0f));

    auto& pt = scene.GetComponent<TransformComponent>(parent);
    auto& ct = scene.GetComponent<TransformComponent>(child);
    EXPECT_NEAR(pt.WorldMatrix[3].x, 5.0f, 1e-5f);
    EXPECT_NEAR(ct.WorldMatrix[3].x, 6.0f, 1e-5f);  // 5 + 1
}

TEST(TransformSystemTest, MultiLevelHierarchyAccumulatesTranslation)
{
    Scene scene;
    WithTransformSystem(scene);
    Entity gp = scene.CreateEntity();   // grandparent
    Entity p  = scene.CreateEntity();   // parent
    Entity c  = scene.CreateEntity();   // child
    scene.SetParent(p, gp);
    scene.SetParent(c, p);
    scene.SetTranslation(gp, glm::vec3(1.0f, 0.0f, 0.0f));
    scene.SetTranslation(p,  glm::vec3(2.0f, 0.0f, 0.0f));
    scene.SetTranslation(c,  glm::vec3(4.0f, 0.0f, 0.0f));
    scene.OnUpdate(Timestep(0.0f));

    auto& ct = scene.GetComponent<TransformComponent>(c);
    EXPECT_NEAR(ct.WorldMatrix[3].x, 7.0f, 1e-5f);  // 1 + 2 + 4
}

// ── Dirty propagation ────────────────────────────────────────────

TEST(SceneECS, SetTranslationPropagatesDirtyToChildren)
{
    Scene scene;
    WithTransformSystem(scene);
    Entity parent = scene.CreateEntity();
    Entity child  = scene.CreateEntity();
    scene.SetParent(child, parent);
    scene.OnUpdate(Timestep(0.0f));   // clear all dirty
    EXPECT_FALSE(scene.GetComponent<TransformComponent>(child).Dirty);

    scene.SetTranslation(parent, glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_TRUE(scene.GetComponent<TransformComponent>(child).Dirty);
}

TEST(TransformSystemTest, DirtyClearedAfterUpdate)
{
    Scene scene;
    WithTransformSystem(scene);
    Entity parent = scene.CreateEntity();
    Entity child  = scene.CreateEntity();
    scene.SetParent(child, parent);
    scene.SetTranslation(parent, glm::vec3(1.0f, 0.0f, 0.0f));
    scene.OnUpdate(Timestep(0.0f));

    EXPECT_FALSE(scene.GetComponent<TransformComponent>(parent).Dirty);
    EXPECT_FALSE(scene.GetComponent<TransformComponent>(child).Dirty);
}

TEST(TransformSystemTest, SecondUpdateWithNoChangesIsStable)
{
    Scene scene;
    WithTransformSystem(scene);
    Entity parent = scene.CreateEntity();
    Entity child  = scene.CreateEntity();
    scene.SetParent(child, parent);
    scene.SetTranslation(parent, glm::vec3(5.0f, 0.0f, 0.0f));
    scene.SetTranslation(child,  glm::vec3(1.0f, 0.0f, 0.0f));
    scene.OnUpdate(Timestep(0.0f));

    float childX = scene.GetComponent<TransformComponent>(child).WorldMatrix[3].x;
    // Second update with no mutations - dirty short-circuit leaves results unchanged.
    scene.OnUpdate(Timestep(0.0f));
    EXPECT_NEAR(scene.GetComponent<TransformComponent>(child).WorldMatrix[3].x, childX, 1e-6f);
}

// ── SetParent: triple chain + cycle rejection ────────────────────

TEST(SceneECS, MultipleChildrenFormSiblingChain)
{
    Scene scene;
    Entity parent = scene.CreateEntity();
    Entity c1 = scene.CreateEntity();
    Entity c2 = scene.CreateEntity();
    Entity c3 = scene.CreateEntity();
    scene.SetParent(c1, parent);
    scene.SetParent(c2, parent);
    scene.SetParent(c3, parent);   // head insertion: FirstChild == c3

    auto& pt = scene.GetComponent<TransformComponent>(parent);
    EXPECT_EQ(pt.FirstChild, c3);
    EXPECT_EQ(scene.GetComponent<TransformComponent>(c3).NextSibling, c2);
    EXPECT_EQ(scene.GetComponent<TransformComponent>(c2).NextSibling, c1);
    EXPECT_EQ(scene.GetComponent<TransformComponent>(c1).NextSibling, NullEntity);
    EXPECT_EQ(scene.GetComponent<TransformComponent>(c3).Parent, parent);
    EXPECT_EQ(scene.GetComponent<TransformComponent>(c1).Parent, parent);
}

TEST(SceneECS, SetParentRejectsCycle)
{
    Scene scene;
    Entity parent = scene.CreateEntity();
    Entity child  = scene.CreateEntity();
    scene.SetParent(child, parent);

    // Would create a cycle (parent's parent = its own descendant) -> rejected.
    scene.SetParent(parent, child);
    EXPECT_EQ(scene.GetComponent<TransformComponent>(parent).Parent, NullEntity);

    // Self-parent rejected; child still points to original parent.
    scene.SetParent(child, child);
    EXPECT_EQ(scene.GetComponent<TransformComponent>(child).Parent, parent);
}

// ── DestroyEntity: orphan children ───────────────────────────────

TEST(SceneECS, DestroyEntityOrphansChildren)
{
    Scene scene;
    Entity parent = scene.CreateEntity();
    Entity child  = scene.CreateEntity();
    scene.SetParent(child, parent);

    scene.DestroyEntity(parent);

    // child survives as an orphan root.
    EXPECT_TRUE(scene.HasComponent<TransformComponent>(child));
    EXPECT_EQ(scene.GetComponent<TransformComponent>(child).Parent, NullEntity);
}
