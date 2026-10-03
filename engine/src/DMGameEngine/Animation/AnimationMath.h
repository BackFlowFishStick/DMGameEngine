/*
 * DMGameEngine - AnimationMath (animation stage 1)
 *
 * Pure-logic sampling math, split out of AnimationSystem so it is testable
 * headless without a Scene/registry (precedent: DeletionQueue property tests).
 * No GPU, no ECS, no assimp - just keyframe streams and matrices.
 *
 * All functions are DMGE_API-exported so engine/tests and tooling can call
 * them across the DLL boundary.
 *
 * Compiled only when DMGE_ANIMATION is ON (engine/CMakeLists.txt gate).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Animation/Skeleton.h"
#include "DMGameEngine/Animation/AnimationClip.h"
#include "glm/glm.hpp"

#include <vector>

namespace DMGameEngine {

namespace AnimationMath {

// ── Keyframe stream sampling ────────────────────────────────────────
// Clamp-to-endpoint semantics: time before the first / after the last key
// returns the first / last key value. An empty stream returns defaultValue.
// Adjacent keys with identical times collapse to the earlier key (t = 0).
DMGE_API glm::vec3 SampleVectorKeys(const std::vector<VectorKey>& keys,
                                    float time, const glm::vec3& defaultValue);

DMGE_API glm::quat SampleQuatKeys(const std::vector<QuatKey>& keys,
                                  float time, const glm::quat& defaultValue);

// Linear interpolation / shortest-path slerp with clamped t (exposed for
// tests and future blending work).
DMGE_API glm::vec3 LerpClamped(const glm::vec3& a, const glm::vec3& b, float t);
DMGE_API glm::quat SlerpShortest(const glm::quat& a, const glm::quat& b, float t);

// ── Clip -> local matrices ─────────────────────────────────────────
// Samples every channel of 'clip' at 'timeTicks' (clip-tick domain) and
// composes per-joint local TRS matrices. Joints without a channel keep the
// identity matrix. outLocal is resized to skeleton.Joints.size().
DMGE_API void SampleLocalMatrices(const AnimationClip& clip,
                                  const Skeleton& skeleton,
                                  float timeTicks,
                                  std::vector<glm::mat4>& outLocal);

// ── Hierarchy propagation + skinning palette ───────────────────────
// world[j] = world[parent] * local[j] (forward pass; import guarantees
// ParentIndex < j). Palette[j] = world[j] * InverseBind[j] - the matrix the
// vertex shader blends with bone weights. outPalette is resized to
// skeleton.Joints.size(); outLocal must have the same size.
DMGE_API void ComputeSkinningPalette(const Skeleton& skeleton,
                                     const std::vector<glm::mat4>& localMatrices,
                                     std::vector<glm::mat4>& outPalette);

// Compose a local TRS matrix (T * R * S) - shared by sampler, importer and
// tests so the composition order lives in exactly one place.
DMGE_API glm::mat4 ComposeTRS(const glm::vec3& translation,
                              const glm::quat& rotation,
                              const glm::vec3& scale);

} // namespace AnimationMath

} // namespace DMGameEngine
