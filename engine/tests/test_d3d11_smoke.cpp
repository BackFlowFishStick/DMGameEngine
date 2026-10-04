/*
 * DMGameEngine - Direct3D 11 headless smoke tests (stage A)
 *
 * Proves the third backend's whole pipeline - device -> resources -> shader
 * -> draw -> CPU readback - without any window. This is the D3D11 answer to
 * kb/KB-07 K-014 (Vulkan surface creation needs an interactive desktop
 * session) and K-021 (headless tests must not go through the backend
 * factories, which on GL dereference a null context): D3D11CreateDevice is
 * legal without a window, and the WARP software rasterizer keeps the test
 * green on machines without a usable GPU. The test constructs the D3D11
 * backend classes DIRECTLY (no Renderer::API / factory wiring exists yet -
 * that is the integrator's 3-line change, DIRECTX_BACKEND_DESIGN.md §7).
 *
 * K-022 honored: zero external asset files - the HLSL sources are embedded
 * as raw string literals below.
 */

#include <gtest/gtest.h>

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Platform/DirectX/DirectXIntegration.h"
#include "DMGameEngine/Platform/DirectX/D3D11RendererAPI.h"
#include "DMGameEngine/Platform/DirectX/D3D11Shader.h"
#include "DMGameEngine/Platform/DirectX/D3D11VertexBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11IndexBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11VertexArray.h"
#include "DMGameEngine/Platform/DirectX/D3D11Texture2D.h"
#include "DMGameEngine/Platform/DirectX/D3D11FrameBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <array>
#include <cstring>

using namespace DMGameEngine;

namespace {

// ── Embedded HLSL (smoke-test subset; names match the GLSL conventions) ──

constexpr const char* kSmokeVertexShader = R"HLSL(
cbuffer VertexConstants : register(b0)
{
    column_major float4x4 u_ViewProjection;
    column_major float4x4 u_Transform;
};

struct VSInput
{
    float3 a_Position : POSITION;
    float4 a_Color    : COLOR;
};

// NOTE: the varyings MUST be declared BEFORE SV_Position in the VS output
// struct. This machine's D3D11 runtime links PS inputs by register ORDER and
// SV_Position would otherwise occupy register 0 - the PS would then receive
// the clip position instead of the varying (kb/KB-07 K-023).
struct VSOutput
{
    float4 v_Color    : TEXCOORD0;
    float4 v_Position : SV_Position;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(u_Transform, float4(input.a_Position, 1.0));
    output.v_Position = mul(u_ViewProjection, worldPos);
    output.v_Color    = input.a_Color;
    return output;
}
)HLSL";

constexpr const char* kSmokePixelShader = R"HLSL(
struct PSInput
{
    float4 v_Color : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
    return input.v_Color;
}
)HLSL";

// ── Fixtures ─────────────────────────────────────────────────────

// A renderer API instance with an initialized headless device. Driver type
// is hardware when available, WARP otherwise (CI/sandbox safe).
class D3D11DeviceEnv
{
public:
    D3D11DeviceEnv()
    {
        // Engine logs are not initialized by gtest_main (same as the
        // test_animation_* suites) - backend code logs, so set it up once.
        static bool logReady = false;
        if (!logReady) { Log::Init(); logReady = true; }

        RendererAPIInitConfig config;
        config.ClearColor      = glm::vec4(0.1f, 0.2f, 0.3f, 1.0f);
        config.DepthTestEnabled = true;
        config.DepthFunction   = DepthFunc::Less;
        config.Culling         = CullMode::None;
        m_API = DirectX::CreateDirectXRendererAPI();
        m_API->Init(config);
    }

    static const D3D11DeviceEnv& Get()
    {
        static D3D11DeviceEnv env;
        return env;
    }

    RendererAPI* Api() const { return m_API.get(); }
    const char* DriverName() const
    {
        return static_cast<const D3D11RendererAPI*>(m_API.get())->GetDriverTypeName();
    }

private:
    DM::Scope<RendererAPI> m_API;
};

struct SmokeVertex
{
    glm::vec3 Position;
    glm::vec4 Color;
};

BufferLayout SmokeLayout()
{
    return { { { ShaderDataType::Float3, "a_Position" },
               { ShaderDataType::Float4, "a_Color"    } } };
}

constexpr uint32_t kFBWidth  = 512;
constexpr uint32_t kFBHeight = 512;

// NDC -> framebuffer pixel (D3D11: framebuffer row 0 is the TOP row;
// NDC y points up, exactly like clip space in GL - see design doc 3.3).
uint32_t PixelX(float ndcX) { return static_cast<uint32_t>((ndcX * 0.5f + 0.5f) * kFBWidth); }
uint32_t PixelY(float ndcY) { return static_cast<uint32_t>((1.0f - (ndcY * 0.5f + 0.5f)) * kFBHeight); }

// Reads one RGBA8 pixel out of the framebuffer color attachment.
std::array<uint8_t, 4> ReadPixel(D3D11FrameBuffer& fb, uint32_t x, uint32_t y)
{
    static std::array<uint8_t, 4> pixels[kFBWidth * kFBHeight];
    static bool cached = false;
    if (!cached)
    {
        if (!fb.CopyColorToStaging() || !fb.ReadbackColor(pixels, sizeof(pixels)))
        {
            ADD_FAILURE() << "framebuffer readback failed";
            static const std::array<uint8_t, 4> zero{};
            return zero;
        }
        cached = true;
    }
    return pixels[y * kFBWidth + x];
}

} // anonymous namespace


// ── Smoke 1: full pipeline - device -> shader -> draw -> readback ──

TEST(D3D11Smoke, TriangleDrawAndReadback)
{
    // Forces the device env (created lazily on first use).
    auto& env = D3D11DeviceEnv::Get();
    ASSERT_NE(D3D11Backend::Device(), nullptr);
    ASSERT_NE(D3D11Backend::Context(), nullptr);
    DMGE_LOG_INFO("[D3D11Smoke] driver in use: {0}", env.DriverName());

    auto* api = static_cast<D3D11RendererAPI*>(env.Api());

    // ── Offscreen target (512x512 RGBA8 + depth) ──
    FramebufferSpecification fbSpec;
    fbSpec.Width  = kFBWidth;
    fbSpec.Height = kFBHeight;
    fbSpec.Attachments = { { TextureFormat::RGBA8 } };
    fbSpec.DepthFormat = TextureFormat::Depth;
    D3D11FrameBuffer framebuffer(fbSpec);

    // ── Shader + geometry ──
    D3D11Shader shader("D3D11Smoke", kSmokeVertexShader, kSmokePixelShader);
    ASSERT_NE(shader.GetVertexBlob(), nullptr);

    // Triangle centered below the NDC origin; u_ViewProjection/u_Transform
    // stay identity, so NDC == clip coordinates (z = 0.5 passes DepthFunc::Less
    // against a cleared 1.0 in D3D's [0,1] depth range).
    const SmokeVertex vertices[3] = {
        { glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec3( 0.5f, -0.5f, 0.5f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec3( 0.0f,  0.5f, 0.5f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
    };
    const uint32_t indices[3] = { 0, 1, 2 };

    auto vertexBuffer = DM::CreateRef<D3D11VertexBuffer>(vertices, sizeof(vertices));
    vertexBuffer->SetLayout(SmokeLayout());
    auto indexBuffer = DM::CreateRef<D3D11IndexBuffer>(indices, 3);
    D3D11VertexArray vertexArray;
    vertexArray.AddVertexBuffer(vertexBuffer);
    vertexArray.SetIndexBuffer(indexBuffer);

    // ── Draw one frame ──
    const glm::mat4 identity(1.0f);
    api->SetViewport(0, 0, static_cast<int>(kFBWidth), static_cast<int>(kFBHeight));

    api->BeginRenderPass(&framebuffer);       // binds RT + clears to (0.1,0.2,0.3)
    shader.Bind();
    shader.SetMat4("u_ViewProjection", identity);
    shader.SetMat4("u_Transform", identity);
    api->DrawIndexed(vertexArray);
    api->EndRenderPass();

    // ── Readback assertions ──
    // 1) Triangle interior: pure red proves VS transform, IA wiring, raster
    //    and PS output all really ran on the GPU.
    {
        const auto px = ReadPixel(framebuffer, PixelX(0.0f), PixelY(-0.3f));
        EXPECT_GT(px[0], 200) << "triangle interior R";
        EXPECT_LT(px[1], 40)  << "triangle interior G";
        EXPECT_LT(px[2], 40)  << "triangle interior B";
        EXPECT_EQ(px[3], 255) << "triangle interior A";
    }
    // 2) Far corner: the clear color proves RTV binding + ClearRenderTargetView.
    {
        const auto px = ReadPixel(framebuffer, 10, 10);
        EXPECT_NEAR(px[0], 26, 3) << "corner R (0.1 * 255)";
        EXPECT_NEAR(px[1], 51, 3) << "corner G (0.2 * 255)";
        EXPECT_NEAR(px[2], 77, 3) << "corner B (0.3 * 255)";
    }
    // 3) Just outside the triangle's left edge (same scanline): still clear color.
    {
        const auto px = ReadPixel(framebuffer, PixelX(-0.95f), PixelY(-0.3f));
        EXPECT_LT(px[0], 40) << "outside triangle R";
    }
}

// ── Smoke 2: shader reflection bridging ──────────────────────────

TEST(D3D11Smoke, ShaderReflectionBridgesUniformNames)
{
    auto& env = D3D11DeviceEnv::Get(); (void)env;
    D3D11Shader shader("D3D11SmokeReflection", kSmokeVertexShader, kSmokePixelShader);
    ASSERT_NE(shader.GetVertexBlob(), nullptr);

    // GLSL-style names found at reflected cbuffer offsets (in the VS stage).
    EXPECT_TRUE(shader.HasUniform("u_ViewProjection"));
    EXPECT_TRUE(shader.HasUniform("u_Transform"));
    // Nonexistent uniform tolerated (GL "location -1" semantics).
    EXPECT_FALSE(shader.HasUniform("u_DoesNotExist"));

    // Writing uniforms (including the unknown one) must not crash: the
    // staging write goes to reflection-derived offsets.
    shader.SetMat4("u_ViewProjection", glm::mat4(1.0f));
    shader.SetMat4("u_Transform", glm::mat4(1.0f));
    shader.SetFloat3("u_DoesNotExist", glm::vec3(1.0f)); // warn-once + ignore

    // Bind uploads the staging cbuffers via UpdateSubresource - exercise it.
    shader.Bind();
    shader.Unbind();
}

// ── Smoke 3: texture creation / upload / mip generation ──────────

TEST(D3D11Smoke, Texture2DCreationSetDataAndMipmaps)
{
    auto& env = D3D11DeviceEnv::Get(); (void)env;
    Texture2DSpecification spec;
    spec.Width  = 4;
    spec.Height = 4;
    spec.Format = TextureFormat::RGBA8;
    spec.GenerateMipmaps = true;

    D3D11Texture2D texture(spec);
    EXPECT_EQ(texture.GetWidth(), 4u);
    EXPECT_EQ(texture.GetHeight(), 4u);
    EXPECT_NE(texture.GetRendererID(), 0u);

    // CPU upload of a checker pattern.
    std::array<uint8_t, 4 * 4 * 4> texels{};
    for (uint32_t y = 0; y < 4; ++y)
        for (uint32_t x = 0; x < 4; ++x)
        {
            uint8_t v = ((x + y) % 2 == 0) ? 255 : 0;
            texels[(y * 4 + x) * 4 + 0] = v;
            texels[(y * 4 + x) * 4 + 1] = v;
            texels[(y * 4 + x) * 4 + 2] = v;
            texels[(y * 4 + x) * 4 + 3] = 255;
        }
    texture.SetData(texels.data(), static_cast<uint32_t>(texels.size()));
    texture.GenerateMipmaps();

    // Unit-table registration path (Bind(2) registers this texture's SRV
    // under unit 2 - mirroring glBindTexture(GL_TEXTURE2, ...)).
    texture.Bind(2);
    EXPECT_NE(D3D11Backend::SRVForUnit(2), nullptr);
    texture.Unbind();

    // Unit 31 without a registered SRV must resolve to the white dummy.
    EXPECT_NE(D3D11Backend::SRVForUnit(31), nullptr);
}

// ── Smoke 4: dynamic vertex buffer streaming ─────────────────────

TEST(D3D11Smoke, DynamicVertexBufferSetData)
{
    auto& env = D3D11DeviceEnv::Get(); (void)env;
    D3D11VertexBuffer buffer(64);
    buffer.SetLayout(SmokeLayout());

    std::array<uint8_t, 64> payload{};
    payload.fill(0xAB);
    buffer.SetData(payload.data(), static_cast<uint32_t>(payload.size()));
    // Oversized writes are clamped, not fatal.
    buffer.SetData(payload.data(), 128);

    EXPECT_EQ(buffer.GetLayout().GetStride(), 28u); // Float3 + Float4
}
