/*
 * DMGameEngine - TagComponent (ECS stage 1b)
 *
 * Human-readable name for an entity, surfaced by the editor (stage 2a)
 * Inspector / hierarchy view. Pure data - no logic.
 */
#pragma once
#include <string>
#include <utility>

namespace DMGameEngine {

struct TagComponent
{
    std::string Tag;

    TagComponent() = default;
    TagComponent(const TagComponent&) = default;
    TagComponent& operator=(const TagComponent&) = default;
    explicit TagComponent(std::string tag) : Tag(std::move(tag)) {}
};

} // namespace DMGameEngine