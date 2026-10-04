#type vertex
#version 430 core

// Deferred lighting pass (deferred rendering stage 1): full-screen pass that
// reads the G-buffer (RT0 albedo+spec, RT1 normal+shininess, depth) and
// evaluates the same Blinn-Phong lighting the forward path (BlinnPhong.glsl)
// uses, with IDENTICAL light uniform names so RenderQueue/Renderer can share
// one upload routine (UploadSceneLighting in DeferredRendering.h).
//
// Geometry: a single full-screen triangle (3 vertices, no index buffer) -
// see Renderer's deferred quad. Positions are raw NDC; no view-projection.

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoords;

out vec2 v_TexCoords;

void main()
{
    v_TexCoords = a_TexCoords;
    gl_Position = vec4(a_Position, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 FragColor;

in vec2 v_TexCoords;

// ── G-buffer inputs (slot assignment must match Renderer's lighting pass:
//    0 = RT0 albedo+spec, 1 = RT1 normal+shininess, 2 = depth) ──────────
uniform sampler2D u_GAlbedoSpec;
uniform sampler2D u_GNormalShininess;
uniform sampler2D u_GDepth;

// ── Depth reprojection ──────────────────────────────────────────────
// u_InverseViewProjection is the inverse of the EXACT view-projection the
// G-buffer pass used (including the Vulkan Y-flip, so it is self-consistent).
// u_NdcZMin folds the one real per-API depth difference into a constant:
// OpenGL maps NDC z [-1,1] -> window [0,1], Vulkan uses NDC z [0,1] directly.
// The Renderer sets it to -1.0 (OpenGL) / 0.0 (Vulkan); 2b will remove this.
uniform mat4  u_InverseViewProjection;
uniform float u_NdcZMin = -1.0;

// ── Sky/background passthrough ──────────────────────────────────────
// Pixels with depth == 1.0 (the clear value) had no geometry: forward's
// background is the clear color, so reproduce it instead of lighting
// garbage (cleared RT1 normal is not a valid normal).
uniform vec4 u_SkyColor = vec4(0.0, 0.0, 0.0, 1.0);

// ── Camera ──────────────────────────────────────────────────
uniform vec3 u_CameraPosition;

// ── Ambient ─────────────────────────────────────────────────
uniform vec3  u_AmbientColor;
uniform float u_AmbientIntensity;

// ── Directional Light ───────────────────────────────────────
uniform vec3  u_DirectionalLight_direction;
uniform vec3  u_DirectionalLight_color;
uniform float u_DirectionalLight_intensity;

// ── Point Lights (MAX_POINT_LIGHTS from Light.h) ────────────
uniform int   u_PointLightCount;
uniform vec3  u_PointLights_position[16];
uniform vec3  u_PointLights_color[16];
uniform float u_PointLights_intensity[16];
uniform float u_PointLights_constant[16];
uniform float u_PointLights_linear[16];
uniform float u_PointLights_quadratic[16];

// ── Spot Lights (MAX_SPOT_LIGHTS from Light.h) ──────────────
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
