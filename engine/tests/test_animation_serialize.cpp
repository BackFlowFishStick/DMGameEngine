/*
 * DMGameEngine - AnimatorComponent serialization + AnimationSystem tests
 * (animation stage 1)
 *
 * Round-trip: scene with an AnimatorComponent (Skeleton/Clip referenced by
 * UUID, registered from temp .skel.json/.anim.json files - the loader-path
 * rule from KB-07 K-012) -> Save -> Load -> field equality.
 *
 * AnimationSystem is ticked headlessly (no GPU): playback advance, loop wrap,
 * pause, and palette output size/content.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/SceneSerializer.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Components/IDComponent.h"
#include "DMGameEngine/Scene/Components/AnimatorComponent.h"
#include "DMGameEngine/Scene/Systems/AnimationSystem.h"
#include "DMGameEngine/Asset/AssetManager.h"

#include <glm/glm.hpp>

using namespace DMGameEngine;

namespace {

constexpr const char* kSkelJson = R"({
    "joints": [
        { "name": "root", "parent": -1, "inverseBind": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1] },
        { "name": "child", "parent": 0, "inverseBind": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,-1,0,1] }
    ]
})";

// Root bobbing: translation 0 -> (0,2,0) over 2 ticks at 1 tick/second.
constexpr const char* kClipJson = R"({
    "name": "bob", "duration": 2.0, "ticksPerSecond": 1.0,
    "channels": [ {
        "joint": 0,
        "translations": [ { "time": 0.0, "value": [0,0,0] }, { "time": 2.0, "value": [0,2,0] } ]
    } ]
})";

void WriteFile(const std::filesystem::path& p, const std::string& content)
{
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
}

Entity FindByUUID(Scene& scene, uint64_t uuid)
{
    auto view = scene.GetRegistry().view<IDComponent>();
    for (auto e : view)
        if (view.get<IDComponent>(e).UUID == uuid)
            return static_cast<Entity>(e);
    return NullEntity;
}

// Registers one skeleton + one clip in AssetManager and returns their UUIDs.
struct TestAssets { AssetUUID skeleton; AssetUUID clip; };

TestAssets RegisterTestAssets(const std::string& dirName)
{
    const auto dir = std::filesystem::temp_directory_path() / dirName;
    std::filesystem::remove_all(dir);
    WriteFile(dir / "test.skel.json", kSkelJson);
    WriteFile(dir / "test.anim.json", kClipJson);

    auto& am = AssetManager::Get();
    TestAssets a;
    a.skeleton = am.Register((dir / "test.skel.json").string(), AssetType::Skeleton);
    a.clip     = am.Register((dir / "test.anim.json").string(), AssetType::AnimationClip);
    return a;
}

class AnimationSerializeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static bool logReady = false;
        if (!logReady) { Log::Init(); logReady = true; }
        AssetManager::Get().Clear();
    }
    void TearDown() override
    {
        AssetManager::Get().Clear();
        std::filesystem::remove_all(std::filesystem::temp_directory_path() / "dmge_anim_ser");
        std::filesystem::remove_all(std::filesystem::temp_directory_path() / "dmge_anim_sys");
    }
};

} // namespace

// ── Serialization round-trip ────────────────────────────────────────

TEST_F(AnimationSerializeTest, AnimatorComponentRoundTrip)
{
    const TestAssets assets = RegisterTestAssets("dmge_anim_ser");

    Scene scene;
    Entity e = scene.CreateEntity("AnimatedBox");
    const uint64_t idUUID = scene.GetComponent<IDComponent>(e).UUID;
    auto& ac = scene.AddComponent<AnimatorComponent>(e);
    ac.SkeletonAsset = AssetHandle(assets.skeleton);
    ac.Clips = { AssetHandle(assets.clip) };
    ac.ActiveClip    = 0;
    ac.Playing       = false;    // non-default values must survive the round trip
    ac.Loop          = false;
    ac.PlaybackSpeed = 2.5f;

    const auto sceneFile = std::filesystem::temp_directory_path() / "dmge_anim_ser.scene";
    SceneSerializer::Save(scene, sceneFile.string());

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, sceneFile.string()));
    std::filesystem::remove(sceneFile);

    const Entity le = FindByUUID(loaded, idUUID);
    ASSERT_NE(le, NullEntity);
    ASSERT_TRUE(loaded.HasComponent<AnimatorComponent>(le));
    const auto& lac = loaded.GetComponent<AnimatorComponent>(le);

    // UUID identity round-trips exactly.
    EXPECT_EQ(lac.SkeletonAsset.GetUUID(), assets.skeleton);
    ASSERT_EQ(lac.Clips.size(), 1u);
    EXPECT_EQ(lac.Clips[0].GetUUID(), assets.clip);
    // Playback state round-trips.
    EXPECT_EQ(lac.ActiveClip, 0);
    EXPECT_FALSE(lac.Playing);
    EXPECT_FALSE(lac.Loop);
    EXPECT_FLOAT_EQ(lac.PlaybackSpeed, 2.5f);
    // Runtime-only fields are not serialized: palette empty on load.
    EXPECT_TRUE(lac.Palette.empty());
}

TEST_F(AnimationSerializeTest, AnimatorReferencesResolveAfterRoundTrip)
{
    const TestAssets assets = RegisterTestAssets("dmge_anim_ser");

    Scene scene;
    Entity e = scene.CreateEntity("AnimatedBox");
    const uint64_t idUUID = scene.GetComponent<IDComponent>(e).UUID;
    auto& ac = scene.AddComponent<AnimatorComponent>(e);
    ac.SkeletonAsset = AssetHandle(assets.skeleton);
    ac.Clips = { AssetHandle(assets.clip) };

    const auto sceneFile = std::filesystem::temp_directory_path() / "dmge_anim_ser2.scene";
    SceneSerializer::Save(scene, sceneFile.string());
    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, sceneFile.string()));
    std::filesystem::remove(sceneFile);

    // The persisted UUIDs still load real resources through AssetManager.
    const Entity le = FindByUUID(loaded, idUUID);
    ASSERT_NE(le, NullEntity);
    ASSERT_TRUE(loaded.HasComponent<AnimatorComponent>(le));
    const auto& lac = loaded.GetComponent<AnimatorComponent>(le);

    auto skeleton = AssetManager::Get().Load<Skeleton>(lac.SkeletonAsset);
    ASSERT_NE(skeleton, nullptr);
    EXPECT_EQ(skeleton->Joints.size(), 2u);
    auto clip = AssetManager::Get().Load<AnimationClip>(lac.Clips[0]);
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->Name, "bob");
}

// ── AnimationSystem headless tick ───────────────────────────────────

TEST_F(AnimationSerializeTest, SystemAdvancesTimeAndProducesPalette)
{
    const TestAssets assets = RegisterTestAssets("dmge_anim_sys");

    Scene scene;
    scene.AddSystem(DM::CreateRef<AnimationSystem>(scene));
    Entity e = scene.CreateEntity("AnimatedBox");
    auto& ac = scene.AddComponent<AnimatorComponent>(e);
    ac.SkeletonAsset = AssetHandle(assets.skeleton);
    ac.Clips = { AssetHandle(assets.clip) };
    // clip: duration 2.0 ticks, TicksPerSecond 1.0 -> 1 second == 1 tick.

    scene.OnUpdate(Timestep(1.0f));
    EXPECT_FLOAT_EQ(ac.CurrentTime, 1.0f);

    // Palette: 2 joints; root translated (0,1,0) at t=1.0 (midpoint of the
    // two keys), inverse binds are trivial for the root -> world slot shows it.
    ASSERT_EQ(ac.Palette.size(), 2u);
    EXPECT_NEAR(ac.Palette[0][3].y, 1.0f, 1e-4);
    // Child joint has inverse bind translate(0,-1,0) and no channel: world is
    // root world (0,1,0); palette = world * invBind = identity.
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_NEAR(ac.Palette[1][c][r], (c == r) ? 1.0f : 0.0f, 1e-4);
}

TEST_F(AnimationSerializeTest, SystemLoopsAndPauses)
{
    const TestAssets assets = RegisterTestAssets("dmge_anim_sys");

    Scene scene;
    scene.AddSystem(DM::CreateRef<AnimationSystem>(scene));
    Entity e = scene.CreateEntity("A");
    auto& ac = scene.AddComponent<AnimatorComponent>(e);
    ac.SkeletonAsset = AssetHandle(assets.skeleton);
    ac.Clips = { AssetHandle(assets.clip) };

    // Loop wrap: 3 seconds over a 2-tick clip -> t = 1.0.
    scene.OnUpdate(Timestep(3.0f));
    EXPECT_FLOAT_EQ(ac.CurrentTime, 1.0f);

    // Pause: time frozen.
    ac.Playing = false;
    scene.OnUpdate(Timestep(0.5f));
    EXPECT_FLOAT_EQ(ac.CurrentTime, 1.0f);
}

TEST_F(AnimationSerializeTest, SystemIgnoresIncompleteAnimator)
{
    Scene scene;
    scene.AddSystem(DM::CreateRef<AnimationSystem>(scene));
    Entity e = scene.CreateEntity("NoAssets");
    auto& ac = scene.AddComponent<AnimatorComponent>(e);   // no skeleton, no clips

    // Must not crash and must not produce a palette.
    scene.OnUpdate(Timestep(0.1f));
    EXPECT_TRUE(ac.Palette.empty());
}
