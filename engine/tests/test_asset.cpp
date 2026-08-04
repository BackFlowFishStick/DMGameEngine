/*
 * DMGameEngine - AssetManager unit tests (pure logic, no GPU / no resource files)
 *
 * Covers Register (UUID assignment + idempotency), GetUUID/GetMetadata lookup,
 * and LoadRegistry/SaveRegistry round-trip. Load<T> dedup needs real resource
 * files (shader/texture), so it is validated via the demo, not here.
 */
#include <gtest/gtest.h>

#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Asset/AssetTypes.h"

#include <filesystem>
#include <string>

using namespace DMGameEngine;

// Fixture: start each test with a clean singleton (registry + cache wiped).
class AssetManagerTest : public ::testing::Test
{
protected:
    void SetUp() override { AssetManager::Get().Clear(); }
};

TEST_F(AssetManagerTest, RegisterAssignsUUIDAndMetadata)
{
    auto& am = AssetManager::Get();
    AssetUUID uuid = am.Register("assets/textures/wood.png", AssetType::Texture2D);
    EXPECT_NE(uuid, NullUUID);
    EXPECT_EQ(am.GetUUID("assets/textures/wood.png"), uuid);

    const auto* meta = am.GetMetadata(uuid);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->Path, "assets/textures/wood.png");
    EXPECT_EQ(meta->Type, AssetType::Texture2D);
}

TEST_F(AssetManagerTest, RegisterIsIdempotentByPath)
{
    auto& am = AssetManager::Get();
    AssetUUID u1 = am.Register("assets/shaders/flat.glsl", AssetType::Shader);
    AssetUUID u2 = am.Register("assets/shaders/flat.glsl", AssetType::Shader);
    EXPECT_EQ(u1, u2);  // same path -> same UUID (no duplicate)
}

TEST_F(AssetManagerTest, GetUUIDUnregisteredReturnsNull)
{
    auto& am = AssetManager::Get();
    EXPECT_EQ(am.GetUUID("nonexistent/path.png"), NullUUID);
    EXPECT_EQ(am.GetMetadata(12345), nullptr);
}

TEST_F(AssetManagerTest, RegistryRoundTripPreservesUUIDs)
{
    auto& am = AssetManager::Get();
    AssetUUID u1 = am.Register("assets/a.png", AssetType::Texture2D);
    AssetUUID u2 = am.Register("assets/b.glsl", AssetType::Shader);

    const std::string regPath = "test_asset_registry.json";
    ASSERT_TRUE(am.SaveRegistry(regPath));

    am.Clear();  // simulate fresh start
    ASSERT_TRUE(am.LoadRegistry(regPath));

    // UUIDs preserved across save/load
    EXPECT_EQ(am.GetUUID("assets/a.png"), u1);
    EXPECT_EQ(am.GetUUID("assets/b.glsl"), u2);

    const auto* m1 = am.GetMetadata(u1);
    ASSERT_NE(m1, nullptr);
    EXPECT_EQ(m1->Path, "assets/a.png");
    EXPECT_EQ(m1->Type, AssetType::Texture2D);

    std::filesystem::remove(regPath);  // cleanup
}

TEST_F(AssetManagerTest, CleanUnusedDoesNotTouchRegistry)
{
    auto& am = AssetManager::Get();
    am.Register("assets/x.png", AssetType::Texture2D);
    am.CleanUnused();  // no resources loaded; sweeps empty cache
    // registry unaffected (CleanUnused only sweeps m_Cache, not m_Registry)
    EXPECT_NE(am.GetUUID("assets/x.png"), NullUUID);
}

TEST_F(AssetManagerTest, ClearWipesEverything)
{
    auto& am = AssetManager::Get();
    AssetUUID u = am.Register("assets/y.png", AssetType::Texture2D);
    am.Clear();
    EXPECT_EQ(am.GetUUID("assets/y.png"), NullUUID);
    EXPECT_EQ(am.GetMetadata(u), nullptr);
}