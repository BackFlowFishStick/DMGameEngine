/*
 * DMGameEngine - IDComponent (ECS stage 1b)
 *
 * Stable UUID for entity identity. Reserved for stage 1c serialization
 * (entity identity survives save/load). Not used for runtime lookup at
 * this stage (the registry owns Entity -> version mapping).
 */
#pragma once
#include <cstdint>

namespace DMGameEngine {

struct IDComponent
{
    uint64_t UUID = 0;

    IDComponent() = default;
    explicit IDComponent(uint64_t uuid) : UUID(uuid) {}
};

} // namespace DMGameEngine