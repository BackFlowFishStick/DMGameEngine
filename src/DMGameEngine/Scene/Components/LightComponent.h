/*
 * DMGameEngine - LightComponent (ECS)
 *
 * Attachable to any entity with a TransformComponent. LightSystem reads
 * these each frame, converts them to backend-agnostic light data structs
 * (DirectionalLightData / PointLightData / SpotLightData), and stores
 * them in Renderer's SceneLightData for RenderQueue::Flush to upload.
 *
 * Direction convention:
 *   Directional / Spot lights point along the entity's local -Z axis
 *   (OpenGL forward). LightSystem extracts the world-space forward vector
 *   from the entity's WorldMatrix.
 *
 * Serialization: stored as a "Light" component in .scene files (see
 * SceneSerializer). All fields are plain data, no runtime-only cache.
 */
#pragma once

#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"
#include <cstdint>

namespace DMGameEngine {

struct DMGE_API LightComponent
{
    enum class Type : uint8_t
    {
        Directional = 0,
        Point,
        Spot
    };

    Type      LightType        = Type::Directional;
    glm::vec3 Color            { 1.0f, 1.0f, 1.0f };
    float     Intensity        = 1.0f;

    // ── Point / Spot attenuation ────────────────────────────
    float     Constant         = 1.0f;
    float     Linear           = 0.09f;
    float     Quadratic        = 0.032f;

    // ── Spot cone angles (degrees) ──────────────────────────
    float     InnerConeAngle   = 12.5f;
    float     OuterConeAngle   = 17.5f;

    // ── Ambient (driven by the first directional light) ─────
    float     AmbientIntensity = 0.15f;

    LightComponent() = default;
    LightComponent(const LightComponent&) = default;
    LightComponent& operator=(const LightComponent&) = default;
};

} // namespace DMGameEngine
