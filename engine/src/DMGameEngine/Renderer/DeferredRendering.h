/*
 * DMGameEngine - Deferred Rendering Support (stage 1, minimal viable)
 *
 * Pure-logic support for the configurable Forward/Deferred render path
 * (ROADMAP 3e stage 1; design: documents/DEFERRED_RENDERING_DESIGN.md).
 * Everything here is backend-agnostic (R5) and GPU-free so it can be
 * unit-tested headless; the GPU orchestration lives in Renderer.cpp and
 * RenderQueue::FlushDeferred.
 *
 * Contents:
 *   - RenderPath            : Forward/Deferred path selector (public API;
 *                             consumed via Renderer::SetRenderPath).
 *   - DeferredPathState     : the request/active snapshot pair that makes
 *                             path switches take effect at BeginScene, not
 *                             mid-frame (pure logic, unit-tested).
 *   - GBufferLayout         : single source of truth for the G-buffer
 *                             attachment table (formats, counts, sampler
 *                             slots) shared by the shaders, the renderer
 *                             orchestration and the tests.
 *   - ShouldRebuildGBuffer  : resize-vs-reuse decision for the G-buffer
 *                             FrameBuffer (pure logic, unit-tested).
 *   - FullscreenTriVertex   : the 3-vertex full-screen triangle the lighting
 *                             pass draws (no index buffer).
 *   - DeferredShaderSet     : the internal shader bundle FlushDeferred needs.
 *   - UploadSceneLighting   : per-name light uniform upload, extracted from
 *                             RenderQueue.cpp so the forward flush and the
 *                             deferred lighting pass share one routine
 *                             (identical uniform names in BlinnPhong.glsl
 *                             and DeferredLighting.glsl).
 *   - Embedded per-stage shader sources for the four deferred shaders. The
 *     engine creates them via Shader::Create(name, vert, frag) so the DLL
 *     has no CWD/filesystem dependency; engine/shaders/{GBuffer,
 *     GBufferSkinned,GBufferInstanced,DeferredLighting}.glsl are combined-
 *     stage reference copies kept in sync (see design doc section 7 - 2b
 *     will unify this).
 *
 * NOTE (2b progress): stage 1 of the 2b convergence landed - RenderPassDesc
 * (Renderer/RenderPassDesc.h) exists, the G-buffer pass is built from it and
 * MatchesGBufferLayout asserts desc/layout consistency; u_NdcZMin is
 * backend-annotated via the desc. Still owed: SPIR-V reflection, unified
 * shader asset path, per-target blend. See design doc section 7.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Light.h"
#include "DMGameEngine/Renderer/RenderPassDesc.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Texture.h"      // TextureFormat
#include "glm/glm.hpp"

#include <cstdint>
#include <string>

namespace DMGameEngine {

// ── Render Path selector ───────────────────────────────────────────
// Forward is the default and the long-term fallback; Deferred routes the
// scene through a G-buffer pass + a full-screen lighting pass (see design
// doc). Selected via Renderer::SetRenderPath, effective from the next
// BeginScene (mid-frame switches are deliberately not allowed - a frame
// must not start as forward and end as deferred).
enum class RenderPath : uint8_t
{
    Forward  = 0,
    Deferred = 1,
};

// ── Path state machine (pure logic) ────────────────────────────────
// Requested = what the user asked for (SetRenderPath); Active = snapshot
// taken at BeginScene that the whole frame runs under. Keeping the two
// apart makes a mid-frame switch a no-op for the current frame.
struct DeferredPathState
{
    RenderPath Requested = RenderPath::Forward;
    RenderPath Active    = RenderPath::Forward;

    void Request(RenderPath path) { Requested = path; }   // effective next BeginScene
    void OnBeginScene()           { Active = Requested; } // frame-locked snapshot
    bool IsDeferredFrame()  const { return Active == RenderPath::Deferred; }
};

// ── G-buffer layout (single source of truth) ───────────────────────
// Attachment table; the shader MRT outputs (GBuffer.glsl, GBufferSkinned.glsl,
// GBufferInstanced.glsl) and the lighting pass sampler slots MUST match.
struct GBufferLayout
{
    static constexpr uint32_t kColorAttachmentCount = 2;

    // Shader sampler slots the lighting pass binds:
    static constexpr uint32_t kAlbedoSpecSlot      = 0;    // RT0
    static constexpr uint32_t kNormalShininessSlot = 1;    // RT1
    static constexpr uint32_t kDepthSlot           = 2;    // depth attachment

    // Uniform names the lighting shader uses for those slots.
    static constexpr const char* kAlbedoSpecSampler      = "u_GAlbedoSpec";
    static constexpr const char* kNormalShininessSampler = "u_GNormalShininess";
    static constexpr const char* kDepthSampler           = "u_GDepth";

    // Format table (design doc section 3).
    static TextureFormat AlbedoSpecFormat()      { return TextureFormat::RGBA8;   }
    static TextureFormat NormalShininessFormat() { return TextureFormat::RGBA16F; }
    static TextureFormat DepthFormat()           { return TextureFormat::Depth;   }

    static constexpr const char* AlbedoSpecRole      = "albedo.rgb + specular strength (a)";
    static constexpr const char* NormalShininessRole = "world normal.xyz + shininess (w)";
    static constexpr const char* DepthRole           = "depth (reprojected in the lighting pass)";
};

// Consistency assertion between a G-buffer render pass desc and the layout
// table (2b stage 1): the desc the renderer builds for the G-buffer pass
// MUST describe exactly the attachments the shaders write and the lighting
// pass samples. Pure logic - unit-tested headless (test_renderpass.cpp).
inline bool MatchesGBufferLayout(const RenderPassDesc& desc)
{
    return desc.ColorAttachmentCount == GBufferLayout::kColorAttachmentCount
        && desc.Color[GBufferLayout::kAlbedoSpecSlot].Format      == GBufferLayout::AlbedoSpecFormat()
        && desc.Color[GBufferLayout::kNormalShininessSlot].Format == GBufferLayout::NormalShininessFormat()
        && desc.HasDepth
        && desc.Depth.Format == GBufferLayout::DepthFormat();
}

// Resize-vs-reuse decision for the G-buffer FrameBuffer. Zero/absurd wanted
// sizes never trigger a (re)build (parity with the GL FBO K-017 guard);
// otherwise rebuild only on an actual dimension change.
inline bool ShouldRebuildGBuffer(uint32_t currentW, uint32_t currentH,
                                 uint32_t wantedW, uint32_t wantedH)
{
    if (wantedW == 0 || wantedH == 0)
        return false;
    constexpr uint32_t kMaxDim = 16384;
    if (wantedW > kMaxDim || wantedH > kMaxDim)
        return false;
    return currentW != wantedW || currentH != wantedH;
}

// ── Full-screen triangle (lighting pass geometry) ──────────────────
// 3 vertices, NDC positions, no index buffer: covers [-1,1]^2 with UVs
// extending past [0,1] on the far corners so interpolation stays linear.
struct FullscreenTriVertex
{
    glm::vec3 Position;
    glm::vec2 TexCoords;
};

inline constexpr FullscreenTriVertex kFullscreenTri[3] = {
    { glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
    { glm::vec3( 3.0f, -1.0f, 0.0f), glm::vec2(2.0f, 0.0f) },
    { glm::vec3(-1.0f,  3.0f, 0.0f), glm::vec2(0.0f, 2.0f) },
};

// ── Internal shader bundle used by the deferred frame ──────────────
// Built by Renderer (lazily, per active API); handed to
// RenderQueue::FlushDeferred. Fields may be null before the first deferred
// BeginScene; Renderer guarantees a valid set before calling FlushDeferred.
struct DeferredShaderSet
{
    DM::Ref<Shader> Static;      // per-draw geometry (GBuffer)
    DM::Ref<Shader> Skinned;     // per-draw + bone palette (GBufferSkinned)
    DM::Ref<Shader> Instanced;   // instanced batches (GBufferInstanced)
    DM::Ref<Shader> Lighting;    // full-screen lighting (DeferredLighting)
};

// ── Shared per-name light uniform upload ───────────────────────────
// Extracted verbatim from RenderQueue.cpp so the forward flush and the
// deferred lighting pass upload IDENTICAL uniform names. Missing uniforms
// are silently skipped (location -1 -> glUniform no-op per GL spec; the
// Vulkan path only synthesizes UBO members for declared uniforms).
inline void UploadSceneLighting(const DM::Ref<Shader>& shader,
                                const glm::vec3& cameraPosition,
                                const SceneLightData& light)
{
    // Camera
    shader->SetFloat3("u_CameraPosition", cameraPosition);

    // Ambient
    shader->SetFloat3("u_AmbientColor", light.AmbientColor);
    shader->SetFloat("u_AmbientIntensity", light.AmbientIntensity);

    // Directional light
    shader->SetFloat3("u_DirectionalLight_direction", light.DirectionalLight.Direction);
    shader->SetFloat3("u_DirectionalLight_color",
                      light.HasDirectional ? light.DirectionalLight.Color : glm::vec3(0.0f));
    shader->SetFloat("u_DirectionalLight_intensity",
                     light.HasDirectional ? light.DirectionalLight.Intensity : 0.0f);

    // Point lights
    shader->SetInt("u_PointLightCount", static_cast<int>(light.PointLightCount));
    for (uint32_t i = 0; i < light.PointLightCount && i < MAX_POINT_LIGHTS; ++i)
    {
        const auto& pl = light.PointLights[i];
        shader->SetFloat3("u_PointLights_position["     + std::to_string(i) + "]", pl.Position);
        shader->SetFloat3("u_PointLights_color["        + std::to_string(i) + "]", pl.Color);
        shader->SetFloat("u_PointLights_intensity["     + std::to_string(i) + "]", pl.Intensity);
        shader->SetFloat("u_PointLights_constant["      + std::to_string(i) + "]", pl.Constant);
        shader->SetFloat("u_PointLights_linear["        + std::to_string(i) + "]", pl.Linear);
        shader->SetFloat("u_PointLights_quadratic["     + std::to_string(i) + "]", pl.Quadratic);
    }

    // Spot lights
    shader->SetInt("u_SpotLightCount", static_cast<int>(light.SpotLightCount));
    for (uint32_t i = 0; i < light.SpotLightCount && i < MAX_SPOT_LIGHTS; ++i)
    {
        const auto& sl = light.SpotLights[i];
        shader->SetFloat3("u_SpotLights_position["       + std::to_string(i) + "]", sl.Position);
        shader->SetFloat3("u_SpotLights_direction["      + std::to_string(i) + "]", sl.Direction);
        shader->SetFloat3("u_SpotLights_color["          + std::to_string(i) + "]", sl.Color);
        shader->SetFloat("u_SpotLights_intensity["       + std::to_string(i) + "]", sl.Intensity);
        shader->SetFloat("u_SpotLights_constant["        + std::to_string(i) + "]", sl.Constant);
        shader->SetFloat("u_SpotLights_linear["          + std::to_string(i) + "]", sl.Linear);
        shader->SetFloat("u_SpotLights_quadratic["       + std::to_string(i) + "]", sl.Quadratic);
        shader->SetFloat("u_SpotLights_innerCutoffCos["  + std::to_string(i) + "]", sl.InnerCutoffCos);
        shader->SetFloat("u_SpotLights_outerCutoffCos["  + std::to_string(i) + "]", sl.OuterCutoffCos);
    }
}

// ── Embedded per-stage deferred shader sources ─────────────────────
// Keep content in sync with engine/shaders/*.glsl (combined-stage copies).
namespace DeferredShaderSources {

// GBuffer.glsl - vertex stage
inline const char* GBufferVS() { return R"DMGLSL(#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform mat4 u_NormalMatrix;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoords;

void main()
{
    vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal = normalize(mat3(u_NormalMatrix) * a_Normal);
    v_TexCoords = a_TexCoords;
    gl_Position = u_ViewProjection * worldPos;
}
)DMGLSL"; }

// GBuffer.glsl - fragment stage (MRT outputs must match GBufferLayout:
// RT0 RGBA8 = albedo.rgb + specular strength; RT1 RGBA16F = world normal +
// shininess)
inline const char* GBufferFS() { return R"DMGLSL(#version 430 core

layout(location = 0) out vec4 o_AlbedoSpec;
layout(location = 1) out vec4 o_NormalShininess;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoords;

// Material (same uniform names as BlinnPhong: FlushDeferred reads the
// values off the queued Material and applies them to this shader)
uniform sampler2D u_AlbedoTexture;
uniform vec3  u_AlbedoColor       = vec3(1.0);
uniform float u_SpecularStrength   = 0.5;
uniform float u_Shininess          = 32.0;
uniform int   u_UseTexture         = 0;

void main()
{
    vec3 albedo = u_AlbedoColor;
    if (u_UseTexture == 1)
        albedo *= texture(u_AlbedoTexture, v_TexCoords).rgb;

    o_AlbedoSpec      = vec4(albedo, u_SpecularStrength);
    o_NormalShininess = vec4(normalize(v_Normal), u_Shininess);
}
)DMGLSL"; }

// GBufferSkinned.glsl - vertex stage (u_BoneMatrices is OpenGL-only at this
// stage, same restriction as the forward BlinnPhongSkinned.glsl)
inline const char* GBufferSkinnedVS() { return R"DMGLSL(#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform mat4 u_NormalMatrix;
uniform mat4 u_BoneMatrices[64];

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoords;

void main()
{
    vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal = normalize(mat3(u_NormalMatrix) * a_Normal);
    v_TexCoords = a_TexCoords;
    gl_Position = u_ViewProjection * worldPos;
}
)DMGLSL"; }

inline const char* GBufferSkinnedFS() { return R"DMGLSL(#version 430 core

layout(location = 0) out vec4 o_AlbedoSpec;
layout(location = 1) out vec4 o_NormalShininess;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoords;

uniform sampler2D u_AlbedoTexture;
uniform vec3  u_AlbedoColor       = vec3(1.0);
uniform float u_SpecularStrength   = 0.5;
uniform float u_Shininess          = 32.0;
uniform int   u_UseTexture         = 0;

void main()
{
    vec3 albedo = u_AlbedoColor;
    if (u_UseTexture == 1)
        albedo *= texture(u_AlbedoTexture, v_TexCoords).rgb;

    o_AlbedoSpec      = vec4(albedo, u_SpecularStrength);
    o_NormalShininess = vec4(normalize(v_Normal), u_Shininess);
}
)DMGLSL"; }

// GBufferInstanced.glsl - vertex stage (per-instance model matrix at
// attribute locations 0-3, mesh attributes at 4+ - matches
// MeshRenderSystem's instanced VA layout)
inline const char* GBufferInstancedVS() { return R"DMGLSL(#version 430 core

layout(location = 0) in mat4 a_InstanceModel;

layout(location = 4) in vec3 a_Position;
layout(location = 5) in vec3 a_Normal;
layout(location = 6) in vec2 a_TexCoords;

uniform mat4 u_ViewProjection;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoords;

void main()
{
    vec4 worldPos = a_InstanceModel * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    // Normal matrix computed per-vertex from the instance model matrix.
    v_Normal = normalize(mat3(transpose(inverse(a_InstanceModel))) * a_Normal);
    v_TexCoords = a_TexCoords;
    gl_Position = u_ViewProjection * worldPos;
}
)DMGLSL"; }

inline const char* GBufferInstancedFS() { return R"DMGLSL(#version 430 core

layout(location = 0) out vec4 o_AlbedoSpec;
layout(location = 1) out vec4 o_NormalShininess;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoords;

uniform sampler2D u_AlbedoTexture;
uniform vec3  u_AlbedoColor       = vec3(1.0);
uniform float u_SpecularStrength   = 0.5;
uniform float u_Shininess          = 32.0;
uniform int   u_UseTexture         = 0;

void main()
{
    vec3 albedo = u_AlbedoColor;
    if (u_UseTexture == 1)
        albedo *= texture(u_AlbedoTexture, v_TexCoords).rgb;

    o_AlbedoSpec      = vec4(albedo, u_SpecularStrength);
    o_NormalShininess = vec4(normalize(v_Normal), u_Shininess);
}
)DMGLSL"; }

// DeferredLighting.glsl - vertex stage (full-screen triangle in raw NDC)
inline const char* DeferredLightingVS() { return R"DMGLSL(#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoords;

out vec2 v_TexCoords;

void main()
{
    v_TexCoords = a_TexCoords;
    gl_Position = vec4(a_Position, 1.0);
}
)DMGLSL"; }

// DeferredLighting.glsl - fragment stage: reads the G-buffer (slot 0 = RT0
// albedo+spec, 1 = RT1 normal+shininess, 2 = depth) and evaluates the same
// Blinn-Phong lighting as the forward path, with IDENTICAL light uniform
// names so UploadSceneLighting serves both.
inline const char* DeferredLightingFS() { return R"DMGLSL(#version 430 core

layout(location = 0) out vec4 FragColor;

in vec2 v_TexCoords;

// G-buffer inputs (slot assignment must match Renderer's lighting pass)
uniform sampler2D u_GAlbedoSpec;
uniform sampler2D u_GNormalShininess;
uniform sampler2D u_GDepth;

// Depth reprojection: u_InverseViewProjection is the inverse of the EXACT
// view-projection the G-buffer pass used (including the Vulkan Y-flip, so
// it is self-consistent). u_NdcZMin folds the one real per-API depth
// difference into a constant: OpenGL maps NDC z [-1,1] -> window [0,1],
// Vulkan uses NDC z [0,1] directly. The value comes from the G-buffer
// RenderPassDesc (backend-annotated: GL -1 / Vulkan 0); 2b stage 1 removed
// the Renderer-layer API branch.
uniform mat4  u_InverseViewProjection;
uniform float u_NdcZMin = -1.0;

// Sky/background passthrough: pixels with depth == 1.0 (the clear value)
// had no geometry - reproduce the forward path's clear color instead of
// lighting garbage (a cleared RT1 normal is not a valid normal).
uniform vec4 u_SkyColor = vec4(0.0, 0.0, 0.0, 1.0);

// Camera
uniform vec3 u_CameraPosition;

// Ambient
uniform vec3  u_AmbientColor;
uniform float u_AmbientIntensity;

// Directional Light
uniform vec3  u_DirectionalLight_direction;
uniform vec3  u_DirectionalLight_color;
uniform float u_DirectionalLight_intensity;

// Point Lights (MAX_POINT_LIGHTS from Light.h)
uniform int   u_PointLightCount;
uniform vec3  u_PointLights_position[16];
uniform vec3  u_PointLights_color[16];
uniform float u_PointLights_intensity[16];
uniform float u_PointLights_constant[16];
uniform float u_PointLights_linear[16];
uniform float u_PointLights_quadratic[16];

// Spot Lights (MAX_SPOT_LIGHTS from Light.h)
uniform int   u_SpotLightCount;
uniform vec3  u_SpotLights_position[8];
uniform vec3  u_SpotLights_direction[8];
uniform vec3  u_SpotLights_color[8];
uniform float u_SpotLights_intensity[8];
uniform float u_SpotLights_constant[8];
uniform float u_SpotLights_linear[8];
uniform float u_SpotLights_quadratic[8];
uniform float u_SpotLights_innerCutoffCos[8];
uniform float u_SpotLights_outerCutoffCos[8];

void main()
{
    float depth = texture(u_GDepth, v_TexCoords).r;
    if (depth >= 0.9999)
    {
        FragColor = u_SkyColor;
        return;
    }

    // Reconstruct world position from the depth attachment (see the uniform
    // comments: ndc.xy = uv*2-1 holds for BOTH APIs under their own texture
    // conventions; only the z range differs, folded into u_NdcZMin).
    vec3 ndc;
    ndc.xy = v_TexCoords * 2.0 - 1.0;
    ndc.z = mix(u_NdcZMin, 1.0, depth);
    vec4 worldHom = u_InverseViewProjection * vec4(ndc, 1.0);
    vec3 worldPos = worldHom.xyz / worldHom.w;

    vec3 normal = normalize(texture(u_GNormalShininess, v_TexCoords).xyz);
    vec4 albedoSpec = texture(u_GAlbedoSpec, v_TexCoords);
    vec3 albedo = albedoSpec.rgb;
    float specularStrength = albedoSpec.a;
    float shininess = max(texture(u_GNormalShininess, v_TexCoords).w, 1.0);

    vec3 viewDir = normalize(u_CameraPosition - worldPos);

    // Ambient (once)
    vec3 result = u_AmbientColor * u_AmbientIntensity * albedo;

    // Directional light (no attenuation)
    if (length(u_DirectionalLight_color) > 0.0)
    {
        vec3 lightDir = normalize(-u_DirectionalLight_direction);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = u_DirectionalLight_color * u_DirectionalLight_intensity * diff * albedo;
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
        vec3 specular = u_DirectionalLight_color * u_DirectionalLight_intensity * specularStrength * spec;
        result += diffuse + specular;
    }

    // Point lights
    for (int i = 0; i < u_PointLightCount && i < 16; ++i)
    {
        vec3 toLight = u_PointLights_position[i] - worldPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);
        float attenuation = 1.0 / (u_PointLights_constant[i]
                                 + u_PointLights_linear[i] * dist
                                 + u_PointLights_quadratic[i] * dist * dist);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = u_PointLights_color[i] * u_PointLights_intensity[i] * diff * albedo * attenuation;
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
        vec3 specular = u_PointLights_color[i] * u_PointLights_intensity[i] * specularStrength * spec * attenuation;
        result += diffuse + specular;
    }

    // Spot lights
    for (int i = 0; i < u_SpotLightCount && i < 8; ++i)
    {
        vec3 toLight = u_SpotLights_position[i] - worldPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);
        float attenuation = 1.0 / (u_SpotLights_constant[i]
                                 + u_SpotLights_linear[i] * dist
                                 + u_SpotLights_quadratic[i] * dist * dist);

        float theta = dot(lightDir, normalize(-u_SpotLights_direction[i]));
        float epsilon = u_SpotLights_innerCutoffCos[i] - u_SpotLights_outerCutoffCos[i];
        float spotIntensity = clamp((theta - u_SpotLights_outerCutoffCos[i]) / max(epsilon, 0.001), 0.0, 1.0);

        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = u_SpotLights_color[i] * u_SpotLights_intensity[i] * diff * albedo * attenuation * spotIntensity;
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
        vec3 specular = u_SpotLights_color[i] * u_SpotLights_intensity[i] * specularStrength * spec * attenuation * spotIntensity;
        result += diffuse + specular;
    }

    FragColor = vec4(result, 1.0);
}
)DMGLSL"; }

} // namespace DeferredShaderSources

} // namespace DMGameEngine
