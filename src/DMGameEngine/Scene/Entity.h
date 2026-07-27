/*
 * DMGameEngine - Entity (ECS stage 1b)
 *
 * Entity is an ID, not an object: it holds no data, no methods, no
 * parent/child pointers. Identity + versioning is owned by the registry
 * (Scene wraps entt::registry). DMGE uses its own alias rather than
 * exposing entt headers directly in the public API (see ECS_DESIGN.md
 * section 12 - strategy A, Hazel-style).
 */
#pragma once
#include <cstdint>

namespace DMGameEngine {

using Entity = uint32_t;
constexpr Entity NullEntity = static_cast<Entity>(-1);

} // namespace DMGameEngine