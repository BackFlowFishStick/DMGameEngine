/*
 * DMGameEngine - per-frame deletion queue scheduling tests (pure logic)
 *
 * Regression tests for review item B (VulkanBackendReview.md): GPU objects
 * destroyed while frames are in flight must not be destroyed earlier than
 * every referencing frame has completed.
 *
 * The scheduling rule under test (VulkanDeletionQueue.h) is header-only and
 * Vulkan-include-free, so these run headless on any configuration. The GPU
 * side (queue mechanics, actual vkDestroy calls) is exercised by runtime
 * smoke tests / Validation Layer, not here.
 *
 * Model (matches VulkanGraphicsContext):
 *   - 2 frame slots, frame f uses slot (f-1) % 2.
 *   - BeginFrame(g) waits the fence of slot(g) (confirming that slot's last
 *     submission = frame g-2), then flushes bucket slot(g).
 *   - A frame's submission therefore is confirmed complete during
 *     BeginFrame(f+2), before that frame's bucket flush runs.
 *   - An object destroyed while frame f is the newest frame that may
 *     reference it (frames are recorded/submitted in order and the app
 *     drops its reference first) is safe to destroy exactly when every
 *     frame <= f has completed, i.e. no earlier than BeginFrame(f+2)'s
 *     fence wait.
 */

#include <gtest/gtest.h>

#include "DMGameEngine/Platform/Vulkan/VulkanDeletionQueue.h"

using DMGameEngine::Detail::DeletionBucketFor;
using DMGameEngine::Detail::kDeletionBucketCount;

namespace {

constexpr uint32_t SlotOfFrame(uint64_t frame)
{
    return static_cast<uint32_t>((frame - 1) % kDeletionBucketCount);
}

// Where is the frame loop when a destructor runs?
enum class EWhen
{
    DuringRecording,   // frame f is being recorded (frame started)
    BetweenFrames      // after EndFrame(f), before BeginFrame(f+1)
};

// First BeginFrame(g) that runs strictly after the destruction point and
// flushes the given bucket (a bucket is flushed by BeginFrame iff
// SlotOfFrame(g) == bucket).
uint64_t FirstFlushFrame(uint64_t destroyedDuringFrame, EWhen when, uint32_t bucket)
{
    const uint64_t firstBeginFrame =
        (when == EWhen::DuringRecording) ? destroyedDuringFrame + 1  // BeginFrame(f) already ran
                                         : destroyedDuringFrame + 1; // BeginFrame(f+1) is next
    uint64_t g = firstBeginFrame;
    while (SlotOfFrame(g) != bucket)
        ++g;
    return g;
}

} // anonymous namespace

// ── Bucket selection sanity ───────────────────────────────────────

TEST(DeletionQueue, BucketAlwaysInRange)
{
    for (uint32_t frame = 0; frame < 64; ++frame)
    {
        EXPECT_LT(DeletionBucketFor(false, frame), kDeletionBucketCount);
        EXPECT_LT(DeletionBucketFor(true, frame), kDeletionBucketCount);
    }
}

TEST(DeletionQueue, TwoSlotsMatchFramesInFlight)
{
    // The whole derivation assumes a ring of 2 in-flight frames.
    EXPECT_EQ(kDeletionBucketCount, 2u);
}

// ── Core safety property ──────────────────────────────────────────
// For every destruction point, the bucket flush that will destroy the
// object must happen at BeginFrame(f+2) or later (all frames <= f completed)
// and within one full slot rotation (no unbounded leak).

TEST(DeletionQueue, DestroyedDuringRecordingFlushesOnlyAfterReferencingFramesComplete)
{
    for (uint64_t f = 1; f <= 16; ++f)
    {
        const uint32_t bucket = DeletionBucketFor(true, SlotOfFrame(f));
        const uint64_t flush = FirstFlushFrame(f, EWhen::DuringRecording, bucket);

        // Frame f completes during BeginFrame(f+2)'s fence wait; earlier
        // frames complete no later. The destroy must not run before that.
        EXPECT_GE(flush, f + 2) << "frame " << f;
        // Bounded deferral: at most one extra slot rotation.
        EXPECT_LE(flush, f + 3) << "frame " << f;
    }
}

TEST(DeletionQueue, DestroyedBetweenFramesFlushesOnlyAfterReferencingFramesComplete)
{
    // Destruction happens after EndFrame(f); frames <= f may still be
    // referenced (both slots can have pending submissions).
    for (uint64_t f = 1; f <= 16; ++f)
    {
        // Between frames, m_CurrentFrame already advanced to the NEXT slot.
        const uint32_t bucket = DeletionBucketFor(false, SlotOfFrame(f + 1));
        const uint64_t flush = FirstFlushFrame(f, EWhen::BetweenFrames, bucket);

        EXPECT_GE(flush, f + 2) << "frame " << f;
        EXPECT_LE(flush, f + 3) << "frame " << f;
    }
}

// ── Concrete regression values ────────────────────────────────────
// Pin the exact rule so a refactor cannot silently flip it
// (e.g. "always current bucket" is unsafe between frames: the previous
// frame on the OTHER slot may still be in flight and its fence is not
// waited before the current bucket's flush).

TEST(DeletionQueue, ConcreteBucketValues)
{
    // Slot of frame f is (f-1) % 2 (frame 1 -> slot 0, frame 2 -> slot 1...).
    // Destroyed while recording a frame on slot 0 -> bucket 0.
    EXPECT_EQ(DeletionBucketFor(true, 0), 0u);
    // Destroyed while recording frame on slot 1 -> bucket 1.
    EXPECT_EQ(DeletionBucketFor(true, 1), 1u);
    // Destroyed between frames, next slot 0 -> bucket 1 (the OTHER slot:
    // its flush confirms the last submission made before the destruction).
    EXPECT_EQ(DeletionBucketFor(false, 0), 1u);
    EXPECT_EQ(DeletionBucketFor(false, 1), 0u);
}
