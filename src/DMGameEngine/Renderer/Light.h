/*
 * DMGameEngine - Light Data Structures
 *
 * Backend-agnostic containers for light parameters collected by
 * LightSystem and uploaded to shaders by RenderQueue::Flush.
 *
 * These are pure data structs (no virtual methods, no backend
 * dependency) so they can be included from both the Renderer and
 * Scene layers without pulling in OpenGL/Vulkan headers.
 *
 * Lighting stage A+B: basic forward Blinn-Phong.
 */
#pragma once

#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"
#include <array>
#include <cstdint>

namespace DMGameEngine {

// Maximum dynamic lights supported per frame. Kept small so per-name
// uniform uploads (no UBO yet) stay within a few dozen SetFloat3 calls.
constexpr uint32_t MAX_POINT_LIGHTS = 16;
constexpr uint32_t MAX_SPOT_LIGHTS  = 8;

// ── Directional Light ───────────────────────────────────────────
// Infinite-distance parallel light (e.g. sun). Only direction matters;
// position is irrelevant. The first directional light in the scene also
// drives the ambient term.
struct DMGE_API DirectionalLightData
{
    glm::vec3 Direction { -0.2f, -1.0f, -0.3f };  // world-space, normalized
    glm::vec3 Color     { 1.0f, 1.0f, 1.0f };
    float     Intensity = 1.0f;
};

// ── Point Light ─────────────────────────────────────────────────
// Omnidirectional light with distance attenuation (constant/linear/quadratic).
struct DMGE_API PointLightData
{
    glm::vec3 Position  { 0.0f, 0.0f, 0.0f };   // world-space
    glm::vec3 Color     { 1.0f, 1.0f, 1.0f };
    float     Intensity = 1.0f;
    float     Constant  = 1.0f;                  // attenuation coefficients
    float     Linear    = 0.09f;
    float     Quadratic = 0.032f;
};

// ── Spot Light ──────────────────────────────────────────────────
// Cone-shaped light: point light + directional cone (inner/outer cutoff).
struct DMGE_API SpotLightData
{
    glm::vec3 Position       { 0.0f, 0.0f, 0.0f };
    glm::vec3 Direction       { 0.0f, 0.0f, -1.0f };
    glm::vec3 Color           { 1.0f, 1.0f, 1.0f };
    float     Intensity       = 1.0f;
    float     Constant        = 1.0f;
    float     Linear          = 0.09f;
    float     Quadratic       = 0.032f;
    float     InnerCutoffCos  = 0.9763f;  // cos(radians(12.5))
    float     OuterCutoffCos  = 0.9530f;  // cos(radians(17.5))
};

// ── Scene Light Data ────────────────────────────────────────────
// Aggregated light state for one frame, populated by LightSystem and
// consumed by RenderQueue::Flush. The Renderer owns a static instance
// (like SceneData) so LightSystem::OnRender fills it and Flush reads it.
struct DMGE_API SceneLightData
{
    DirectionalLightData DirectionalLight;
    bool                 HasDirectional = false;

    uint32_t PointLightCount = 0;
    uint32_t SpotLightCount  = 0;

    std::array<PointLightData, MAX_POINT_LIGHTS> PointLights;
    std::array<SpotLightData, MAX_SPOT_LIGHTS>   SpotLights;

    glm::vec3 AmbientColor     { 0.15f, 0.15f, 0.15f };
    float     AmbientIntensity = 1.0f;

    void Clear()
    {
        HasDirectional    = false;
        PointLightCount   = 0;
        SpotLightCount    = 0;
        AmbientColor      = { 0.15f, 0.15f, 0.15f };
        AmbientIntensity  = 1.0f;
        DirectionalLight  = {};
        PointLights.fill({});
        SpotLights.fill({});
    }
};

} // namespace DMGameEngine
