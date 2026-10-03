/*
 * DMGameEngine - Skeleton (animation stage 1)
 *
 * Flat-array joint hierarchy + per-joint inverse bind matrix. Bones are NOT
 * entities: joint count is small but every joint moves every frame, so the
 * hierarchy is a contiguous array with integer parent links and is recomputed
 * wholesale each frame (see ECS_DESIGN.md - the Entity/dirty hierarchy is
 * explicitly not used for skeletons).
 *
 * Coordinate spaces:
 *   InverseBindMatrix maps mesh-space (bind pose) -> joint space. The skinning
 *   palette is computed as world[j] * InverseBindMatrix[j] (AnimationMath).
 *
 * Compiled only when DMGE_ANIMATION is ON (engine/CMakeLists.txt gate).
 * assimp types stay inside the importer TU (R1 - no assimp in public headers).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <cstdint>

namespace DMGameEngine {

// One joint (bone) of a Skeleton. ParentIndex refers to another joint's index
// in Skeleton::Joints (-1 = root). Import guarantees ParentIndex < the joint's
// own index (parents first) so hierarchy propagation is a single forward pass.
struct DMGE_API Joint
{
    std::string Name;
    int32_t     ParentIndex       = -1;
    glm::mat4   InverseBindMatrix{1.0f};   // mesh space -> joint space (bind pose)
};

class DMGE_API Skeleton
{
public:
    std::vector<Joint> Joints;

    uint32_t GetJointCount() const { return static_cast<uint32_t>(Joints.size()); }

    // Linear search by joint name; returns -1 when not found.
    int32_t FindJointIndex(const std::string& name) const;
};

} // namespace DMGameEngine
