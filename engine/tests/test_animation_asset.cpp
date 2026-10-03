/*
 * DMGameEngine - Skeleton / AnimationClip asset tests (animation stage 1)
 *
 * Part A (self-contained): minimal inline .skel.json / .anim.json written to
 * the temp dir and loaded through AssetManager - the loader-path rule from
 * KB-07 K-012 (programmatic assets must have a real asset identity to be
 * serializable).
 *
 * Part B (assimp): drives a real skinned model through AssetLoader<Mesh>'s
 * assimp dispatch and verifies the derived "#skeleton"/"#anim/<i>" assets.
 * Uses the vendored assimp test model in the MAIN checkout (gitignored bulk,
 * absolute path precedent from test_assimp_import.cpp); skipped gracefully
 * when the file is absent.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Asset/AssetTypes.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Asset/Mesh.h"
#include "DMGameEngine/Animation/Skeleton.h"
#include "DMGameEngine/Animation/AnimationClip.h"
#include "DMGameEngine/Animation/AnimationMath.h"

using namespace DMGameEngine;

namespace {

// Small self-contained glTF (embedded buffers) copied into tests/assets so
// the suite runs on any checkout (CI has no assimp test tree, see .gitignore).
#ifndef DMGE_TEST_ASSETS_DIR
#define DMGE_TEST_ASSETS_DIR "."
#endif
constexpr const char* kSkinModelPath =
    DMGE_TEST_ASSETS_DIR "/simple_skin.gltf";

void WriteFile(const std::filesystem::path& p, const std::string& content)
{
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
}

class AnimationAssetTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static bool logReady = false;
        if (!logReady) { Log::Init(); logReady = true; }
        AssetManager::Get().Clear();
    }
    void TearDown() override { AssetManager::Get().Clear(); }
};

} // namespace

// ── Part A: inline JSON formats ─────────────────────────────────────

TEST_F(AnimationAssetTest, SkeletonJsonRoundTripFields)
{
    const auto dir = std::filesystem::temp_directory_path() / "dmge_anim_json";
    std::filesystem::remove_all(dir);
    WriteFile(dir / "two.skel.json", R"({
        "joints": [
            { "name": "root", "parent": -1, "inverseBind": [1,0,0,0, 0,1,0,0, 0,0,1,0, -1,0,0,1] },
            { "name": "child", "parent": 0, "inverseBind": [1,0,0,0, 0,1,0,0, 0,0,1,0, -2,-3,0,1] }
        ]
    })");

    auto& am = AssetManager::Get();
    const AssetUUID uuid = am.Register((dir / "two.skel.json").string(), AssetType::Skeleton);
    auto skeleton = am.Load<Skeleton>(uuid);

    ASSERT_NE(skeleton, nullptr);
    ASSERT_EQ(skeleton->Joints.size(), 2u);
    EXPECT_EQ(skeleton->Joints[0].Name, "root");
    EXPECT_EQ(skeleton->Joints[0].ParentIndex, -1);
    EXPECT_EQ(skeleton->Joints[1].Name, "child");
    EXPECT_EQ(skeleton->Joints[1].ParentIndex, 0);
    // Inverse bind survived the file round-trip: mesh->joint of child is
    // translate(-2,-3,0).
    EXPECT_EQ(skeleton->Joints[1].InverseBindMatrix[3], glm::vec4(-2.0f, -3.0f, 0.0f, 1.0f));
}

TEST_F(AnimationAssetTest, ClipJsonRoundTripFields)
{
    const auto dir = std::filesystem::temp_directory_path() / "dmge_anim_json";
    WriteFile(dir / "wave.anim.json", R"({
        "name": "wave", "duration": 2.5, "ticksPerSecond": 12.5,
        "channels": [ {
            "joint": 0,
            "translations": [ { "time": 0.0, "value": [0,0,0] }, { "time": 2.5, "value": [5,0,0] } ],
            "rotations":    [ { "time": 0.0, "value": [0,0,0,1] } ],
            "scales":       [ { "time": 0.0, "value": [1,1,1] } ]
        } ]
    })");

    auto& am = AssetManager::Get();
    const AssetUUID uuid = am.Register((dir / "wave.anim.json").string(), AssetType::AnimationClip);
    auto clip = am.Load<AnimationClip>(uuid);

    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->Name, "wave");
    EXPECT_FLOAT_EQ(clip->Duration, 2.5f);
    EXPECT_FLOAT_EQ(clip->TicksPerSecond, 12.5f);
    ASSERT_EQ(clip->Channels.size(), 1u);
    EXPECT_EQ(clip->Channels[0].JointIndex, 0);
    ASSERT_EQ(clip->Channels[0].Translations.size(), 2u);
    ASSERT_EQ(clip->Channels[0].Rotations.size(), 1u);
    EXPECT_EQ(clip->Channels[0].Rotations[0].Value, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
}

// ── Part B: assimp skinned import ───────────────────────────────────

TEST_F(AnimationAssetTest, AssimpSkinnedImportProducesMeshSkeletonAndClip)
{
    if (!std::filesystem::exists(kSkinModelPath))
        GTEST_SKIP() << "assimp test model not available (gitignored bulk)";

    auto& am = AssetManager::Get();
    const AssetUUID meshUUID = am.Register(kSkinModelPath, AssetType::Mesh);
    auto mesh = am.Load<Mesh>(meshUUID);
    ASSERT_NE(mesh, nullptr);

    // Mesh grew bone attributes (superset layout, in order).
    bool hasIndices = false, hasWeights = false;
    for (const auto& el : mesh->Layout.GetElements())
    {
        if (el.Name == "a_BoneIndices") hasIndices = true;
        if (el.Name == "a_BoneWeights") hasWeights = true;
    }
    EXPECT_TRUE(hasIndices);
    EXPECT_TRUE(hasWeights);
    EXPECT_FALSE(mesh->Vertices.empty());
    EXPECT_FALSE(mesh->Indices.empty());

    // Derived skeleton asset registered + loads with valid hierarchy.
    const AssetUUID skelUUID = am.GetUUID(std::string(kSkinModelPath) + "#skeleton");
    ASSERT_NE(skelUUID, NullUUID);
    auto skeleton = am.Load<Skeleton>(skelUUID);
    ASSERT_NE(skeleton, nullptr);
    EXPECT_GT(skeleton->Joints.size(), 0u);
    for (size_t i = 0; i < skeleton->Joints.size(); ++i)
        EXPECT_LT(skeleton->Joints[i].ParentIndex, static_cast<int32_t>(i))
            << "parent-first invariant violated at joint " << i;
    // At least one non-root joint exists in a skinned model; its inverse bind
    // should not be identity (it maps mesh space into the joint's bind frame).
    bool nonTrivialInverseBind = false;
    for (const auto& j : skeleton->Joints)
        if (j.ParentIndex >= 0 && j.InverseBindMatrix != glm::mat4(1.0f))
            nonTrivialInverseBind = true;
    EXPECT_TRUE(nonTrivialInverseBind);

    // Derived clip asset registered + loads with channels.
    const AssetUUID clipUUID = am.GetUUID(std::string(kSkinModelPath) + "#anim/0");
    ASSERT_NE(clipUUID, NullUUID);
    auto clip = am.Load<AnimationClip>(clipUUID);
    ASSERT_NE(clip, nullptr);
    EXPECT_GT(clip->Duration, 0.0f);
    EXPECT_FALSE(clip->Channels.empty());

    // Integration: the imported clip samples against the imported skeleton.
    std::vector<glm::mat4> local, palette;
    AnimationMath::SampleLocalMatrices(*clip, *skeleton, 0.0f, local);
    AnimationMath::ComputeSkinningPalette(*skeleton, local, palette);
    EXPECT_EQ(palette.size(), skeleton->Joints.size());
}

TEST_F(AnimationAssetTest, AssimpDerivedSkeletonPathIsStableAcrossLoads)
{
    if (!std::filesystem::exists(kSkinModelPath))
        GTEST_SKIP() << "assimp test model not available (gitignored bulk)";

    auto& am = AssetManager::Get();
    auto mesh = am.Load<Mesh>(kSkinModelPath);
    ASSERT_NE(mesh, nullptr);

    const AssetUUID first = am.GetUUID(std::string(kSkinModelPath) + "#skeleton");
    ASSERT_NE(first, NullUUID);
    // Re-registering the same derived path must return the same UUID (dedup).
    EXPECT_EQ(am.Register(std::string(kSkinModelPath) + "#skeleton", AssetType::Skeleton), first);
    // Type mismatch is rejected by AssetManager.
    EXPECT_EQ(am.Load<Mesh>(first), nullptr);
}
