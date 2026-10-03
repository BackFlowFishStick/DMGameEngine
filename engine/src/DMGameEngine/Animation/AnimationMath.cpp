/*
 * DMGameEngine - AnimationMath implementation (animation stage 1)
 */
#include "DMGameEngine/Animation/AnimationMath.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include <algorithm>
#include <cmath>

namespace DMGameEngine {

namespace AnimationMath {

glm::vec3 LerpClamped(const glm::vec3& a, const glm::vec3& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return glm::mix(a, b, t);
}

glm::quat SlerpShortest(const glm::quat& a, const glm::quat& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    // glm::slerp negates b when dot < 0, i.e. it already takes the shortest
    // arc; wrapping here just documents the intent.
    return glm::slerp(a, b, t);
}

glm::vec3 SampleVectorKeys(const std::vector<VectorKey>& keys,
                           float time, const glm::vec3& defaultValue)
{
    if (keys.empty()) return defaultValue;
    if (time <= keys.front().Time) return keys.front().Value;
    if (time >= keys.back().Time)  return keys.back().Value;

    for (size_t i = 0; i + 1 < keys.size(); ++i)
    {
        if (time >= keys[i].Time && time <= keys[i + 1].Time)
        {
            const float span = keys[i + 1].Time - keys[i].Time;
            const float t = span > 1e-8f ? (time - keys[i].Time) / span : 0.0f;
            return LerpClamped(keys[i].Value, keys[i + 1].Value, t);
        }
    }
    return keys.back().Value;   // unreachable for sorted keys; defensive
}

glm::quat SampleQuatKeys(const std::vector<QuatKey>& keys,
                         float time, const glm::quat& defaultValue)
{
    if (keys.empty()) return defaultValue;
    if (time <= keys.front().Time) return keys.front().Value;
    if (time >= keys.back().Time)  return keys.back().Value;

    for (size_t i = 0; i + 1 < keys.size(); ++i)
    {
        if (time >= keys[i].Time && time <= keys[i + 1].Time)
        {
            const float span = keys[i + 1].Time - keys[i].Time;
            const float t = span > 1e-8f ? (time - keys[i].Time) / span : 0.0f;
            return SlerpShortest(keys[i].Value, keys[i + 1].Value, t);
        }
    }
    return keys.back().Value;
}

glm::mat4 ComposeTRS(const glm::vec3& translation, const glm::quat& rotation,
                     const glm::vec3& scale)
{
    return glm::translate(glm::mat4(1.0f), translation)
         * glm::mat4_cast(rotation)
         * glm::scale(glm::mat4(1.0f), scale);
}

void SampleLocalMatrices(const AnimationClip& clip, const Skeleton& skeleton,
                         float timeTicks, std::vector<glm::mat4>& outLocal)
{
    outLocal.assign(skeleton.Joints.size(), glm::mat4(1.0f));

    for (const auto& channel : clip.Channels)
    {
        if (channel.JointIndex < 0 ||
            channel.JointIndex >= static_cast<int32_t>(skeleton.Joints.size()))
            continue;   // channel targeting an unknown joint: skip, don't crash

        const glm::vec3 t = SampleVectorKeys(channel.Translations, timeTicks,
                                             glm::vec3(0.0f));
        const glm::quat r = SampleQuatKeys(channel.Rotations, timeTicks,
                                           glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        const glm::vec3 s = SampleVectorKeys(channel.Scales, timeTicks,
                                             glm::vec3(1.0f));
        outLocal[static_cast<size_t>(channel.JointIndex)] = ComposeTRS(t, r, s);
    }
}

void ComputeSkinningPalette(const Skeleton& skeleton,
                            const std::vector<glm::mat4>& localMatrices,
                            std::vector<glm::mat4>& outPalette)
{
    const size_t count = skeleton.Joints.size();

    // Pass 1: propagate world matrices (parents come before children per the
    // import invariant). Uses a scratch world array - outPalette cannot double
    // as the world buffer because a parent's slot would already contain
    // world * InverseBind when its children read it.
    std::vector<glm::mat4> world(count);
    for (size_t i = 0; i < count; ++i)
    {
        const int32_t parent = skeleton.Joints[i].ParentIndex;
        const glm::mat4& local = (i < localMatrices.size())
            ? localMatrices[i] : glm::mat4(1.0f);

        world[i] = (parent >= 0 && static_cast<size_t>(parent) < i)
            ? world[static_cast<size_t>(parent)] * local
            : local;
    }

    // Pass 2: palette[j] = world[j] * InverseBind[j].
    outPalette.resize(count);
    for (size_t i = 0; i < count; ++i)
        outPalette[i] = world[i] * skeleton.Joints[i].InverseBindMatrix;
}

} // namespace AnimationMath

} // namespace DMGameEngine
