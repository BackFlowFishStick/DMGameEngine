/*
 * DMGameEngine - Skeleton implementation (animation stage 1)
 */
#include "DMGameEngine/Animation/Skeleton.h"

namespace DMGameEngine {

int32_t Skeleton::FindJointIndex(const std::string& name) const
{
    for (size_t i = 0; i < Joints.size(); ++i)
        if (Joints[i].Name == name)
            return static_cast<int32_t>(i);
    return -1;
}

} // namespace DMGameEngine
