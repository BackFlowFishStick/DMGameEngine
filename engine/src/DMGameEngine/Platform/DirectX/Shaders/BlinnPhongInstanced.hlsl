// DMGameEngine - Instanced Blinn-Phong HLSL (Direct3D 11 backend)
//
// D3D11 twin of engine/shaders/BlinnPhongInstanced.glsl: identical uniform
// names so the OpenGL-style per-name uploads (Shader::SetXxx) bridge 1:1
// through D3DReflect offsets (documents/DIRECTX_BACKEND_DESIGN.md §5).
//
// Vertex-input semantics are assigned by D3D11VertexArray from the
// BufferElement names: mesh buffer slot 0 (a_Position -> POSITION0,
// a_Normal -> NORMAL0, a_TexCoords -> TEXCOORD0), per-instance buffer
// slot 1 (a_InstanceModel -> TEXCOORD1, a matrix consumes the consecutive
// registers TEXCOORD1..4).
//
// Loaded through Shader::Create(filepath): the file uses the engine's
// `#type vertex` / `#type fragment` block convention, with HLSL content
// (VSMain/PSMain entry points) inside the blocks.
//
// IMPORTANT: declare varyings BEFORE SV_Position in the VS output struct -
// some D3D11 runtimes link PS inputs by register ORDER, and SV_Position
// declared first would occupy register 0 (kb/KB-07 K-025).

#type vertex

cbuffer InstanceConstants : register(b0)
{
    column_major float4x4 u_ViewProjection;
};

struct VSInput
{
    float3  a_Position     : POSITION;
    float3  a_Normal       : NORMAL;
    float2  a_TexCoords    : TEXCOORD0;
    float4x4 a_InstanceModel : TEXCOORD1; // rows in TEXCOORD1..4, per-instance
};

struct VSOutput
{
    float3 v_WorldPos  : TEXCOORD0;
    float3 v_Normal    : TEXCOORD1;
    float2 v_TexCoords : TEXCOORD2;
    float4 v_Position  : SV_Position;
};

// HLSL has no inverse() intrinsic: build the inverse-transpose 3x3 by the
// adjugate (matches the GLSL shader's transpose(inverse(...))).
float3x3 InverseTranspose(float4x4 m)
{
    float3x3 a = (float3x3)m;
    float  det = dot(cross(a[0], a[1]), a[2]);
    float3x3 adj;
    adj[0] = cross(a[1], a[2]);
    adj[1] = cross(a[2], a[0]);
    adj[2] = cross(a[0], a[1]);
    return transpose(adj / det);
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(input.a_InstanceModel, float4(input.a_Position, 1.0));
    output.v_WorldPos  = worldPos.xyz;
    // Normal matrix per-vertex from the instance model matrix (see
    // InverseTranspose - the demo's uniform-scale cubes keep this exact).
    output.v_Normal    = normalize(mul(InverseTranspose(input.a_InstanceModel),
                                       input.a_Normal));
    output.v_TexCoords = input.a_TexCoords;
    output.v_Position  = mul(u_ViewProjection, worldPos);
    return output;
}

#type fragment

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
