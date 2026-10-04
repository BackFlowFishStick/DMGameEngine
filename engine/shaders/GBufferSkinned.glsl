#type vertex
#version 430 core

// G-buffer pass, skinned variant (animation stage 1): identical G-buffer
// contract to GBuffer.glsl, plus the u_BoneMatrices palette upload used by
// SkinnedMeshRenderSystem. u_BoneMatrices is OpenGL-only at this stage
// (see Shader::SetMat4Array for the Vulkan note) - the same restriction the
// forward path has via BlinnPhongSkinned.glsl.

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
