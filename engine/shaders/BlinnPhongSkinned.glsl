#type vertex
#version 430 core

// BlinnPhongSkinned - skeletal-mesh variant of BlinnPhong (animation stage 1).
// Differences from BlinnPhong.glsl (fragment stage is identical):
//   - locations 3/4: a_BoneIndices (stored as Float4, cast to ivec4 here so the
//     interleaved vertex buffer stays float-only) + a_BoneWeights
//   - u_BoneMatrices[128]: per-draw bone palette (world * inverse bind),
//     uploaded by SkinnedMeshRenderSystem via Shader::SetMat4Array
//   - position/normal are skinned by the weighted palette blend before the
//     model transform; zero-weight vertices fall back to the identity
//     (unskinned) transform so degenerate imports stay renderable

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec4 a_BoneIndices;
layout(location = 4) in vec4 a_BoneWeights;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform mat4 u_NormalMatrix;
uniform mat4 u_BoneMatrices[128];

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoords;

void main()
{
    ivec4 boneIndices = ivec4(a_BoneIndices);
    float weightSum = a_BoneWeights.x + a_BoneWeights.y
                    + a_BoneWeights.z + a_BoneWeights.w;

    mat4 skin = mat4(1.0);
    if (weightSum > 0.0)
    {
        skin = a_BoneWeights.x * u_BoneMatrices[boneIndices.x]
             + a_BoneWeights.y * u_BoneMatrices[boneIndices.y]
             + a_BoneWeights.z * u_BoneMatrices[boneIndices.z]
             + a_BoneWeights.w * u_BoneMatrices[boneIndices.w];
    }

    vec4 skinnedPos = skin * vec4(a_Position, 1.0);
    // Palette rows are rotations+translations in mesh space; transforming the
    // normal by the same weighted mat3 and re-normalizing is adequate for
    // stage 1 (uniform-scale-approximate; proper inverse-transpose per bone
    // is a follow-up).
    vec3 skinnedNormal = mat3(skin) * a_Normal;

    vec4 worldPos = u_Transform * skinnedPos;
    v_WorldPos = worldPos.xyz;
    v_Normal = normalize(mat3(u_NormalMatrix) * normalize(skinnedNormal));
    v_TexCoords = a_TexCoords;
    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 430 core

layout(location = 0) out vec4 FragColor;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoords;

// ── Camera ──────────────────────────────────────────────────
uniform vec3 u_CameraPosition;

// ── Material ────────────────────────────────────────────────
uniform sampler2D u_AlbedoTexture;
uniform vec3  u_AlbedoColor       = vec3(1.0);
uniform float u_SpecularStrength   = 0.5;
uniform float u_Shininess          = 32.0;
uniform int   u_UseTexture         = 0;

// ── Ambient ─────────────────────────────────────────────────
uniform vec3  u_AmbientColor;
uniform float u_AmbientIntensity;

// ── Directional Light ───────────────────────────────────────
uniform vec3  u_DirectionalLight_direction;
uniform vec3  u_DirectionalLight_color;
uniform float u_DirectionalLight_intensity;

// ── Point Lights ────────────────────────────────────────────
uniform int   u_PointLightCount;
uniform vec3  u_PointLights_position[16];
uniform vec3  u_PointLights_color[16];
uniform float u_PointLights_intensity[16];
uniform float u_PointLights_constant[16];
uniform float u_PointLights_linear[16];
uniform float u_PointLights_quadratic[16];

// ── Spot Lights ─────────────────────────────────────────────
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
    vec3 normal = normalize(v_Normal);
    vec3 viewDir = normalize(u_CameraPosition - v_WorldPos);

    vec3 albedo = u_AlbedoColor;
    if (u_UseTexture == 1)
        albedo *= texture(u_AlbedoTexture, v_TexCoords).rgb;

    // Ambient (once)
    vec3 result = u_AmbientColor * u_AmbientIntensity * albedo;

    // Directional light (no attenuation)
    if (length(u_DirectionalLight_color) > 0.0)
    {
        vec3 lightDir = normalize(-u_DirectionalLight_direction);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = u_DirectionalLight_color * u_DirectionalLight_intensity * diff * albedo;
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), u_Shininess);
        vec3 specular = u_DirectionalLight_color * u_DirectionalLight_intensity * u_SpecularStrength * spec;
        result += diffuse + specular;
    }

    // Point lights
    for (int i = 0; i < u_PointLightCount && i < 16; ++i)
    {
        vec3 toLight = u_PointLights_position[i] - v_WorldPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);
        float attenuation = 1.0 / (u_PointLights_constant[i]
                                 + u_PointLights_linear[i] * dist
                                 + u_PointLights_quadratic[i] * dist * dist);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = u_PointLights_color[i] * u_PointLights_intensity[i] * diff * albedo * attenuation;
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), u_Shininess);
        vec3 specular = u_PointLights_color[i] * u_PointLights_intensity[i] * u_SpecularStrength * spec * attenuation;
        result += diffuse + specular;
    }

    // Spot lights
    for (int i = 0; i < u_SpotLightCount && i < 8; ++i)
    {
        vec3 toLight = u_SpotLights_position[i] - v_WorldPos;
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
        float spec = pow(max(dot(normal, halfDir), 0.0), u_Shininess);
        vec3 specular = u_SpotLights_color[i] * u_SpotLights_intensity[i] * u_SpecularStrength * spec * attenuation * spotIntensity;
        result += diffuse + specular;
    }

    FragColor = vec4(result, 1.0);
}
