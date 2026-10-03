/*
 * DMGameEngine - AnimationClip (animation stage 1)
 *
 * One named animation: a duration (in ticks, with TicksPerSecond defining the
 * time scale) and per-joint keyframe channels. Each channel targets a joint by
 * index into the Skeleton's joint array; translations/scales are vec3 streams
 * and rotations are quaternion streams, kept as separate keys (assimp's native
 * shape - the three streams may have different key times).
 *
 * Compiled only when DMGE_ANIMATION is ON (engine/CMakeLists.txt gate).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include <string>
#include <vector>
#include <cstdint>

namespace DMGameEngine {

struct DMGE_API VectorKey
{
    float     Time  = 0.0f;        // in clip ticks
    glm::vec3 Value{0.0f};
};

struct DMGE_API QuatKey
{
    float     Time  = 0.0f;        // in clip ticks
    glm::quat Value{1.0f, 0.0f, 0.0f, 0.0f};   // identity
};

// Keyframes for one joint. JointIndex indexes Skeleton::Joints (-1 = unset).
struct DMGE_API AnimationChannel
{
    int32_t                JointIndex = -1;
    std::vector<VectorKey> Translations;
    std::vector<QuatKey>   Rotations;
    std::vector<VectorKey> Scales;
};

class DMGE_API AnimationClip
{
public:
    std::string Name;
    float       Duration       = 0.0f;    // in ticks
    float       TicksPerSecond = 25.0f;
    std::vector<AnimationChannel> Channels;
};

} // namespace DMGameEngine
