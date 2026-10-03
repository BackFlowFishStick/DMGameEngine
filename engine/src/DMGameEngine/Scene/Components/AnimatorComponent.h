/*
 * DMGameEngine - AnimatorComponent (animation stage 1)
 *
 * Pure data (R4): asset references + playback state + runtime output. All
 * animation logic lives in AnimationSystem (sampling) / AnimationMath
 * (interpolation + palette).
 *
 * References the Skeleton and the AnimationClips by AssetHandle UUID only
 * (KB-05 rule 1 - never store paths or Refs in components). Handles are
 * serialized as UUIDs; the runtime resources load lazily via AssetManager
 * during AnimationSystem::OnUpdate.
 *
 * Palette is the per-frame skinning matrix output (world * inverse bind per
 * joint) consumed by SkinnedMeshRenderSystem; it is runtime-only and never
 * serialized. Playing/Loop/PlaybackSpeed/CurrentTime are runtime playback
 * state; CurrentTime is intentionally not serialized (animations restart on
 * scene load).
 *
 * Compiled only when DMGE_ANIMATION is ON.
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "glm/glm.hpp"

#include <vector>
#include <cstdint>

namespace DMGameEngine {

struct AnimatorComponent
{
    // ── Asset references (serialized as UUIDs) ─────────────────
    AssetHandle SkeletonAsset;                // Skeleton resource UUID
    std::vector<AssetHandle> Clips;           // AnimationClip UUIDs

    // ── Playback state ─────────────────────────────────────────
    int   ActiveClip    = 0;                  // index into Clips
    bool  Playing       = true;
    bool  Loop          = true;
    float PlaybackSpeed = 1.0f;
    float CurrentTime   = 0.0f;               // in clip ticks (runtime)

    // ── Runtime output (not serialized) ───────────────────────
    std::vector<glm::mat4> Palette;           // per-joint skinning matrices

    AnimatorComponent() = default;
    AnimatorComponent(const AnimatorComponent&) = default;
    AnimatorComponent& operator=(const AnimatorComponent&) = default;
};

} // namespace DMGameEngine
