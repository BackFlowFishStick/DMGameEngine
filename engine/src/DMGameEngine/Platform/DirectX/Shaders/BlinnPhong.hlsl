// DMGameEngine - Blinn-Phong HLSL (Direct3D 11 backend)
//
// Full uniform-set parity with engine/shaders/BlinnPhong.glsl: every GLSL
// uniform name appears verbatim as a cbuffer member, so the OpenGL-style
// per-name uploads (Shader::SetXxx) bridge 1:1 through D3DReflect offsets
// (see documents/DIRECTX_BACKEND_DESIGN.md §5).
//
// Semantics notes:
//  - column_major matrices: glm uploads column-major bytes, and mul(M, v)
//    computes M*v exactly like GLSL's M * v.
//  - Vertex inputs use engine semantics (POSITION/NORMAL/TEXCOORD/COLOR)
//    assigned by D3D11VertexArray from BufferElement names.
//  - IMPORTANT: declare varyings BEFORE SV_Position in the VS output struct.
//    Some D3D11 runtimes link PS inputs to VS outputs by register ORDER, and
//    SV_Position declared first would occupy register 0 - the PS would then
//    receive the clip position instead of the varyings (kb/KB-07 K-025).
//  - Stage B: this file becomes the D3D11 backend's forward-path shader.
//    The stage-A smoke test embeds a simplified subset inline (K-022: no
//    external test assets).

// ── Vertex shader ────────────────────────────────────────────────

cbuffer VertexConstants : register(b0)
{
    column_major float4x4 u_ViewProjection;
    column_major float4x4 u_Transform;
    column_major float4x4 u_NormalMatrix;
};

struct VSInput
{
    float3 a_Position : POSITION;
    float3 a_Normal   : NORMAL;
    float2 a_TexCoords : TEXCOORD0;
};

struct VSOutput
{
    float3 v_WorldPos : TEXCOORD0;
    float3 v_Normal   : TEXCOORD1;
    float2 v_TexCoords : TEXCOORD2;
    float4 v_Position : SV_Position;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(u_Transform, float4(input.a_Position, 1.0));
    output.v_WorldPos  = worldPos.xyz;
    output.v_Normal    = normalize(mul((float3x3)u_NormalMatrix, input.a_Normal));
    output.v_TexCoords = input.a_TexCoords;
    output.v_Position  = mul(u_ViewProjection, worldPos);
    return output;
}

// ── Pixel shader ─────────────────────────────────────────────────

Texture2D    u_AlbedoTexture        : register(t0);
SamplerState u_AlbedoTextureSampler : register(s0);

cbuffer PixelConstants : register(b0)
{
    float3 u_CameraPosition;

    // Material
    float3 u_AlbedoColor;
    float  u_SpecularStrength;
    float  u_Shininess;
    int    u_UseTexture;

    // Ambient
    float3 u_AmbientColor;
    float  u_AmbientIntensity;

    // Directional light
    float3 u_DirectionalLight_direction;
    float3 u_DirectionalLight_color;
    float  u_DirectionalLight_intensity;

    // Point lights
    int    u_PointLightCount;
    float3 u_PointLights_position[16];
    float3 u_PointLights_color[16];
    float  u_PointLights_intensity[16];
    float  u_PointLights_constant[16];
    float  u_PointLights_linear[16];
    float  u_PointLights_quadratic[16];

    // Spot lights
    int    u_SpotLightCount;
    float3 u_SpotLights_position[8];
    float3 u_SpotLights_direction[8];
    float3 u_SpotLights_color[8];
    float  u_SpotLights_intensity[8];
    float  u_SpotLights_constant[8];
    float  u_SpotLights_linear[8];
    float  u_SpotLights_quadratic[8];
    float  u_SpotLights_innerCutoffCos[8];
    float  u_SpotLights_outerCutoffCos[8];
};

struct PSInput
{
    float3 v_WorldPos  : TEXCOORD0;
    float3 v_Normal    : TEXCOORD1;
    float2 v_TexCoords : TEXCOORD2;
};

float3 BlinnPhong(float3 normal, float3 lightDir, float3 viewDir,
                  float3 lightColor, float lightIntensity, float3 albedo)
{
    float diff = max(dot(normal, lightDir), 0.0);
    float3 diffuse = lightColor * lightIntensity * diff * albedo;
    float3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), u_Shininess);
    float3 specular = lightColor * lightIntensity * u_SpecularStrength * spec;
    return diffuse + specular;
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 normal  = normalize(input.v_Normal);
    float3 viewDir = normalize(u_CameraPosition - input.v_WorldPos);

    float3 albedo = u_AlbedoColor;
    if (u_UseTexture == 1)
        albedo *= u_AlbedoTexture.Sample(u_AlbedoTextureSampler, input.v_TexCoords).rgb;

    // Ambient
    float3 result = u_AmbientColor * u_AmbientIntensity * albedo;

    // Directional light (no attenuation)
    if (length(u_DirectionalLight_color) > 0.0)
    {
        float3 lightDir = normalize(-u_DirectionalLight_direction);
        result += BlinnPhong(normal, lightDir, viewDir,
                             u_DirectionalLight_color, u_DirectionalLight_intensity, albedo);
    }

    // Point lights
    for (int i = 0; i < u_PointLightCount && i < 16; ++i)
    {
        float3 toLight = u_PointLights_position[i] - input.v_WorldPos;
        float dist = length(toLight);
        float3 lightDir = toLight / max(dist, 0.001);
        float attenuation = 1.0 / (u_PointLights_constant[i]
                                 + u_PointLights_linear[i] * dist
                                 + u_PointLights_quadratic[i] * dist * dist);
        result += BlinnPhong(normal, lightDir, viewDir,
                             u_PointLights_color[i],
                             u_PointLights_intensity[i] * attenuation, albedo);
    }

    // Spot lights
    for (int j = 0; j < u_SpotLightCount && j < 8; ++j)
    {
        float3 toLight = u_SpotLights_position[j] - input.v_WorldPos;
        float dist = length(toLight);
        float3 lightDir = toLight / max(dist, 0.001);
        float attenuation = 1.0 / (u_SpotLights_constant[j]
                                 + u_SpotLights_linear[j] * dist
                                 + u_SpotLights_quadratic[j] * dist * dist);

        float theta = dot(lightDir, normalize(-u_SpotLights_direction[j]));
        float epsilon = u_SpotLights_innerCutoffCos[j] - u_SpotLights_outerCutoffCos[j];
        float spotIntensity = clamp((theta - u_SpotLights_outerCutoffCos[j]) / max(epsilon, 0.001), 0.0, 1.0);

        result += BlinnPhong(normal, lightDir, viewDir,
                             u_SpotLights_color[j],
                             u_SpotLights_intensity[j] * attenuation * spotIntensity, albedo);
    }

    return float4(result, 1.0);
}
