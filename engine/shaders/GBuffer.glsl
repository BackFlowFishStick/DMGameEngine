#type vertex
#version 430 core

// G-buffer pass (deferred rendering stage 1): static per-draw geometry pass.
// Outputs the two color attachments declared in DeferredRendering.h
// (GBufferLayout): RT0 = albedo.rgb + specular strength (a), RT1 =
// world-space normal.xyz + shininess (w). Depth comes from the G-buffer's
// depth attachment. Vulkan note: explicit in/out required; locations for
// the in/out vars below are re-emitted by VulkanShader::RewriteStageBody.

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

#type fragment
#version 430 core

// MRT outputs (must match GBufferLayout in Renderer/DeferredRendering.h):
//   RT0 RGBA8  : albedo.rgb + specular strength in alpha
//   RT1 RGBA16F: world normal.xyz + shininess in w
layout(location = 0) out vec4 o_AlbedoSpec;
layout(location = 1) out vec4 o_NormalShininess;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoords;

// ── Material (same uniform names as BlinnPhong: FlushDeferred reads the
//    values off the queued Material and applies them to this shader) ──
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

    o_AlbedoSpec    = vec4(albedo, u_SpecularStrength);
    o_NormalShininess = vec4(normalize(v_Normal), u_Shininess);
}
