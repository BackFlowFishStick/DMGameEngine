/*
 * DMGameEngine - Vulkan per-frame deletion queue: bucket scheduling math
 *
 * Review item B (VulkanBackendReview.md): Vulkan GPU objects (images,
 * buffers, samplers, views, pipelines) must not be destroyed while an
 * in-flight frame submission may still reference them.
 *
 * Scheme: kDeletionBucketCount deletion buckets, one per frame-in-flight
 * slot. The queues themselves live in VulkanDevice (PushDeferDestroy /
 * FlushDeletions); the flush point is VulkanGraphicsContext::BeginFrame,
 * which flushes bucket b right after vkWaitForFences confirmed the last
 * submission made on slot b.
 *
 * This header is intentionally free of any Vulkan includes so the
 * scheduling rule can be unit-tested headless (engine/tests). It only
 * encodes the ring-of-two invariant:
 *
 *   A destroyed object can be referenced only by submissions already
 *   recorded/submitted before the destruction (the app drops its last
 *   reference first, so later frames cannot bind it).
 *
 *   Flush(bucket b) at BeginFrame(slot b) happens after fence b signaled,
 *   i.e. after the LAST submission ever made on slot b before that
 *   BeginFrame. Because fences are waited in slot order 0,1,0,1..., a
 *   bucket flush at BeginFrame(slot b) also observes completion of every
 *   submission made on the other slot in earlier frames.
 *
 *   Therefore an object destroyed while frame f is the newest frame that
 *   may reference it is safe if it lands in the bucket whose next flush
 *   happens at BeginFrame(f + 2) (two slots later, both fences confirmed):
 *
 *   - Frame started (destruction during recording of frame f on slot F):
 *       bucket F. Next flush of bucket F is BeginFrame(f+2) where fence F
 *       confirms frame f; fence F^1 already confirmed frame f-1 at
 *       BeginFrame(f+1).
 *   - Frame not started (destruction between EndFrame(f) and BeginFrame(f+1),
 *       currentFrame = F is the NEXT slot): bucket F^1. Next flush of
 *       bucket F^1 is BeginFrame(f+2) where fence F^1 confirms frame f (the
 *       last submission on slot F^1); fence F already confirmed frame f-1
 *       at BeginFrame(f+1).
 *
 * Both cases: the object is destroyed at BeginFrame(f+2), strictly after
 * every referencing frame completed. See engine/tests/test_deletion_queue.cpp
 * for the property test.
 */

#pragma once

#include <cstdint>

namespace DMGameEngine::Detail {

// Must match VulkanGraphicsContext::kMaxFramesInFlight (static_assert there).
inline constexpr uint32_t kDeletionBucketCount = 2;

// Bucket a deferred-destroy issued right now must land in.
//   frameStarted  : is a frame currently being recorded?
//   currentFrame  : the slot the next BeginFrame will use (or is using).
inline constexpr uint32_t DeletionBucketFor(bool frameStarted, uint32_t currentFrame)
{
    return frameStarted
        ? (currentFrame % kDeletionBucketCount)
        : ((currentFrame + 1) % kDeletionBucketCount);
}

} // namespace DMGameEngine::Detail
