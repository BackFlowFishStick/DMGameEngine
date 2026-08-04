#type vertex
#version 430 core

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

    vec3 result = u_AmbientColor * u_AmbientIntensity * albedo;

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
