/*
 * DMGameEngine - SceneSerializer unit tests (stage 1c)
 *
 * Round-trip: Save -> Load -> field equality. Covers identity (UUID/Tag),
 * transform locals, hierarchy rebuild (parent UUID -> triple chain),
 * MeshComponent asset UUIDs, CameraComponent params, and version field.
 *
 * nlohmann/json is PRIVATE, so this test does not include <nlohmann/json.hpp>;
 * the version-field check reads the raw file text instead.
 */
#include <gtest/gtest.h>

#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/SceneSerializer.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Components/IDComponent.h"
#include "DMGameEngine/Scene/Components/TagComponent.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"
#include "DMGameEngine/Scene/Components/CameraComponent.h"
#include "DMGameEngine/Asset/AssetHandle.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace DMGameEngine;

namespace {
Entity FindByUUID(Scene& scene, uint64_t uuid)
{
    auto view = scene.GetRegistry().view<IDComponent>();
    for (auto e : view)
        if (view.get<IDComponent>(e).UUID == uuid)
            return static_cast<Entity>(e);
    return NullEntity;
}

const std::string kScenePath = "test_scene_serializer.json";
} // namespace

class SceneSerializerTest : public ::testing::Test
{
protected:
    void TearDown() override { std::filesystem::remove(kScenePath); }
};

TEST_F(SceneSerializerTest, RoundTripPreservesIdentity)
{
    Scene scene;
    Entity e = scene.CreateEntity("Player");
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    EXPECT_EQ(loaded.GetComponent<TagComponent>(le).Tag, std::string("Player"));
}

TEST_F(SceneSerializerTest, RoundTripPreservesTransform)
{
    Scene scene;
    Entity e = scene.CreateEntity();
    auto& tc = scene.GetComponent<TransformComponent>(e);
    tc.Translation   = { 1.5f, 2.5f, 3.5f };
    tc.RotationEuler  = { 0.1f, 0.2f, 0.3f };
    tc.Scale          = { 2.0f, 3.0f, 4.0f };
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    auto& ltc = loaded.GetComponent<TransformComponent>(le);
    EXPECT_FLOAT_EQ(ltc.Translation.x, 1.5f);
    EXPECT_FLOAT_EQ(ltc.Translation.y, 2.5f);
    EXPECT_FLOAT_EQ(ltc.Translation.z, 3.5f);
    EXPECT_FLOAT_EQ(ltc.RotationEuler.x, 0.1f);
    EXPECT_FLOAT_EQ(ltc.Scale.x, 2.0f);
    EXPECT_FLOAT_EQ(ltc.Scale.z, 4.0f);
}

TEST_F(SceneSerializerTest, RoundTripRebuildsHierarchy)
{
    Scene scene;
    Entity parent = scene.CreateEntity("Parent");
    Entity child  = scene.CreateEntity("Child");
    scene.SetParent(child, parent);
    uint64_t parentUUID = scene.GetComponent<IDComponent>(parent).UUID;
    uint64_t childUUID  = scene.GetComponent<IDComponent>(child).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity lp = FindByUUID(loaded, parentUUID);
    Entity lc = FindByUUID(loaded, childUUID);
    ASSERT_NE(lp, NullEntity);
    ASSERT_NE(lc, NullEntity);
    // parent rebuilt by UUID: child.Parent == parent
    EXPECT_EQ(loaded.GetComponent<TransformComponent>(lc).Parent, lp);
}

TEST_F(SceneSerializerTest, RoundTripPreservesMeshAssetUUID)
{
    Scene scene;
    Entity e = scene.CreateEntity();
    auto& mc = scene.AddComponent<MeshComponent>(e);
    mc.MeshAsset = AssetHandle(static_cast<uint64_t>(12345));
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    ASSERT_TRUE(loaded.HasComponent<MeshComponent>(le));
    auto& lmc = loaded.GetComponent<MeshComponent>(le);
    EXPECT_EQ(lmc.MeshAsset.GetUUID(), 12345u);
}

TEST_F(SceneSerializerTest, RoundTripPreservesCamera)
{
    Scene scene;
    Entity e = scene.CreateEntity();
    auto& cc = scene.AddComponent<CameraComponent>(e);
    cc.Primary = true;
    cc.Camera.SetPerspectiveVerticalFOV(60.0f);
    cc.Camera.SetProjectionType(SceneCamera::ProjectionType::Orthographic);
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    ASSERT_TRUE(loaded.HasComponent<CameraComponent>(le));
    auto& lcc = loaded.GetComponent<CameraComponent>(le);
    EXPECT_TRUE(lcc.Primary);
    EXPECT_FLOAT_EQ(lcc.Camera.GetPerspectiveVerticalFOV(), 60.0f);
    EXPECT_EQ(lcc.Camera.GetProjectionType(), SceneCamera::ProjectionType::Orthographic);
}

TEST_F(SceneSerializerTest, VersionFieldPresent)
{
    Scene scene;
    scene.CreateEntity();
    SceneSerializer::Save(scene, kScenePath);

    // Read raw file text; nlohmann/json is PRIVATE so we grep instead of parsing.
    std::ifstream fin(kScenePath);
    ASSERT_TRUE(fin.is_open());
    std::string content((std::istreambuf_iterator<char>(fin)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("\"version\""), std::string::npos);
    EXPECT_NE(content.find("\"entities\""), std::string::npos);
}