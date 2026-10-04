#type vertex
#version 430 core

// G-buffer pass, instanced variant: same G-buffer contract as GBuffer.glsl,
// with the per-instance model matrix at attribute locations 0-3 (mirrors
// BlinnPhongInstanced.glsl and MeshRenderSystem's instanced VA layout:
// instance buffer FIRST at locations 0-3, mesh attributes at 4+).

// Per-instance attributes (locations 0-3, from instance buffer, divisor=1)
layout(location = 0) in mat4 a_InstanceModel;

// Per-vertex attributes (locations 4+, from mesh vertex buffer)
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

#type fragment
#version 430 core

// MRT outputs (must match GBufferLayout in Renderer/DeferredRendering.h).
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
