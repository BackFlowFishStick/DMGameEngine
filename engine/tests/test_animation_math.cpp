/*
 * DMGameEngine - AnimationMath unit tests (animation stage 1)
 *
 * Pure-logic tests for the sampling math: keyframe interpolation (clamped
 * endpoints, per-segment lerp/slerp), clip -> local TRS composition, bone
 * hierarchy propagation, and the skinning palette (world * inverse bind).
 * No GPU/window, no Scene - headless like test_deletion_queue.cpp.
 *
 * These run only when DMGE_ANIMATION is ON (tests/CMakeLists.txt gate).
 */
#include <gtest/gtest.h>

#include "DMGameEngine/Animation/Skeleton.h"
#include "DMGameEngine/Animation/AnimationClip.h"
#include "DMGameEngine/Animation/AnimationMath.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

using namespace DMGameEngine;
using namespace DMGameEngine::AnimationMath;

namespace {

// Two joints: root at index 0, child of root at index 1.
Skeleton MakeTwoJointSkeleton()
{
    Skeleton s;
    Joint root;
    root.Name = "root";
    root.ParentIndex = -1;
    root.InverseBindMatrix = glm::mat4(1.0f);
    Joint child;
    child.Name = "child";
    child.ParentIndex = 0;
    child.InverseBindMatrix = glm::mat4(1.0f);
    s.Joints.push_back(root);
    s.Joints.push_back(child);
    return s;
}

// One translation channel on joint 0: 0.0 -> (0,0,0), 2.0 -> (2,0,0).
AnimationClip MakeLinearTranslateClip()
{
    AnimationClip clip;
    clip.Name = "move";
    clip.Duration = 2.0f;
    clip.TicksPerSecond = 24.0f;
    AnimationChannel ch;
    ch.JointIndex = 0;
    ch.Translations = { {0.0f, glm::vec3(0.0f)}, {2.0f, glm::vec3(2.0f, 0.0f, 0.0f)} };
    clip.Channels.push_back(ch);
    return clip;
}

} // namespace

// ── Keyframe stream sampling ────────────────────────────────────────

TEST(AnimationMathTest, VectorKeysClampToEndpoints)
{
    const std::vector<VectorKey> keys = {
        {0.0f, glm::vec3(1.0f)},
        {1.0f, glm::vec3(3.0f)},
    };
    EXPECT_EQ(SampleVectorKeys(keys, -5.0f, glm::vec3(0.0f)), glm::vec3(1.0f));
    EXPECT_EQ(SampleVectorKeys(keys,  2.0f, glm::vec3(0.0f)), glm::vec3(3.0f));
}

TEST(AnimationMathTest, VectorKeysEmptyStreamReturnsDefault)
{
    const glm::vec3 def(4.0f, 5.0f, 6.0f);
    EXPECT_EQ(SampleVectorKeys({}, 0.5f, def), def);
}

TEST(AnimationMathTest, VectorKeysLinearMidpoint)
{
    const std::vector<VectorKey> keys = {
        {0.0f, glm::vec3(0.0f)},
        {2.0f, glm::vec3(4.0f, 8.0f, -2.0f)},
    };
    EXPECT_EQ(SampleVectorKeys(keys, 1.0f, glm::vec3(0.0f)),
              glm::vec3(2.0f, 4.0f, -1.0f));
}

TEST(AnimationMathTest, VectorKeysDegenerateSegmentReturnsEarlierKey)
{
    // Two keys at the same time: the segment has zero span -> t = 0.
    const std::vector<VectorKey> keys = {
        {0.0f, glm::vec3(1.0f)},
        {1.0f, glm::vec3(1.0f)},
        {1.0f, glm::vec3(9.0f)},
        {2.0f, glm::vec3(9.0f)},
    };
    EXPECT_EQ(SampleVectorKeys(keys, 1.0f, glm::vec3(0.0f)), glm::vec3(1.0f));
}

TEST(AnimationMathTest, QuatKeysQuarterTurnMidpoint)
{
    // 0 rad -> pi/2 rotation around Z; midpoint should be pi/4.
    const glm::quat half = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1));
    const glm::quat id(1.0f, 0.0f, 0.0f, 0.0f);
    const std::vector<QuatKey> keys = { {0.0f, id}, {2.0f, half} };

    const glm::quat mid = SampleQuatKeys(keys, 1.0f, id);
    const glm::quat quarter = glm::angleAxis(glm::quarter_pi<float>(), glm::vec3(0, 0, 1));
    // Same rotation up to sign (q and -q are the same rotation): |dot| == 1.
    EXPECT_NEAR(std::abs(glm::dot(mid, quarter)), 1.0f, 1e-5);
}

TEST(AnimationMathTest, QuatKeysEmptyStreamReturnsDefault)
{
    const glm::quat def = glm::angleAxis(0.3f, glm::vec3(0, 1, 0));
    EXPECT_EQ(SampleQuatKeys({}, 0.5f, def), def);
}

// ── Clip -> local matrices ──────────────────────────────────────────

TEST(AnimationMathTest, SampleLocalMatricesInterpolatesTargetJoint)
{
    Skeleton skel = MakeTwoJointSkeleton();
    AnimationClip clip = MakeLinearTranslateClip();

    std::vector<glm::mat4> local;
    SampleLocalMatrices(clip, skel, 1.0f, local);

    ASSERT_EQ(local.size(), skel.Joints.size());
    // Joint 0 at t=1.0: translated (1,0,0).
    EXPECT_EQ(local[0][3], glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    // Joint 1 has no channel: identity.
    EXPECT_EQ(local[1], glm::mat4(1.0f));
}

TEST(AnimationMathTest, SampleLocalMatricesSkipsOutOfRangeChannel)
{
    Skeleton skel = MakeTwoJointSkeleton();
    AnimationClip clip = MakeLinearTranslateClip();
    AnimationChannel bad;
    bad.JointIndex = 42;   // out of range: must be ignored, not crash
    bad.Translations = { {0.0f, glm::vec3(9.0f)} };
    clip.Channels.push_back(bad);

    std::vector<glm::mat4> local;
    SampleLocalMatrices(clip, skel, 0.0f, local);
    EXPECT_EQ(local[0][3], glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

// ── Hierarchy propagation + palette ─────────────────────────────────

TEST(AnimationMathTest, PaletteIdentityInBindPose)
{
    Skeleton skel = MakeTwoJointSkeleton();
    // local == bind pose == identity -> world == identity -> palette = world * invBind = identity.
    std::vector<glm::mat4> local(skel.Joints.size(), glm::mat4(1.0f));
    std::vector<glm::mat4> palette;
    ComputeSkinningPalette(skel, local, palette);

    ASSERT_EQ(palette.size(), skel.Joints.size());
    for (const auto& m : palette)
        EXPECT_EQ(m, glm::mat4(1.0f));
}

TEST(AnimationMathTest, PalettePropagatesParentWorldToChild)
{
    Skeleton skel = MakeTwoJointSkeleton();
    // Root translated (1,0,0); child translated (0,1,0) locally.
    std::vector<glm::mat4> local(skel.Joints.size(), glm::mat4(1.0f));
    local[0] = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    local[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    std::vector<glm::mat4> palette;
    ComputeSkinningPalette(skel, local, palette);

    // Child world = (1,1,0); inverse binds are identity so palette == world.
    EXPECT_EQ(palette[0][3], glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(palette[1][3], glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
}

TEST(AnimationMathTest, PaletteCancelsInverseBindAtBindPose)
{
    // If a joint's bind local is T_bind and InverseBind = inverse(world_bind),
    // then with local = T_bind the palette must be identity (skinning is a
    // no-op in the bind pose - the core invariant of the palette math).
    Skeleton skel;
    Joint a; a.Name = "a"; a.ParentIndex = -1;
    const glm::mat4 bindLocalA = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0, 0));
    const glm::mat4 worldA = bindLocalA;
    a.InverseBindMatrix = glm::inverse(worldA);
    Joint b; b.Name = "b"; b.ParentIndex = 0;
    const glm::mat4 bindLocalB = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0))
                               * glm::mat4_cast(glm::angleAxis(0.7f, glm::vec3(0, 0, 1)));
    const glm::mat4 worldB = worldA * bindLocalB;
    b.InverseBindMatrix = glm::inverse(worldB);
    skel.Joints.push_back(a);
    skel.Joints.push_back(b);

    std::vector<glm::mat4> local = { bindLocalA, bindLocalB };
    std::vector<glm::mat4> palette;
    ComputeSkinningPalette(skel, local, palette);

    for (const auto& m : palette)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                EXPECT_NEAR(m[c][r], (c == r) ? 1.0f : 0.0f, 1e-4);
    }
}

// ── ComposeTRS ──────────────────────────────────────────────────────

TEST(AnimationMathTest, ComposeTRSMatchesTranslateRotateScaleOrder)
{
    const glm::vec3 t(1.0f, 2.0f, 3.0f);
    const glm::quat r = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 1, 0));
    const glm::vec3 s(2.0f);
    const glm::mat4 m = ComposeTRS(t, r, s);
    const glm::mat4 expected = glm::translate(glm::mat4(1.0f), t)
                             * glm::mat4_cast(r)
                             * glm::scale(glm::mat4(1.0f), s);
    EXPECT_EQ(m, expected);
}
