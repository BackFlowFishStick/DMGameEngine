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
#include "DMGameEngine/Platform/DirectX/DirectXGraphicsContext.h"

#include <array>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

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
// the clip position instead of the varying (kb/KB-07 K-025).
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

// ── Smoke 5: indexed array uniforms ("u_PointLights_position[3]") ──
// Stage B forward path uploads per-index light uniforms (UploadSceneLighting
// calls SetFloat3("u_PointLights_position[i]", ...) etc.). HLSL packs float3
// cbuffer array elements at 16-byte strides; the reflection-derived
// ElementStride must place each write at the packed offset.

constexpr const char* kArrayUniformVertexShader = R"HLSL(
cbuffer VertexConstants : register(b0)
{
    column_major float4x4 u_ViewProjection;
    column_major float4x4 u_Transform;
};

struct VSOutput
{
    float4 v_Position : SV_Position;
};

VSOutput VSMain(float3 a_Position : POSITION)
{
    VSOutput output;
    output.v_Position = mul(u_ViewProjection, mul(u_Transform, float4(a_Position, 1.0)));
    return output;
}
)HLSL";

constexpr const char* kArrayUniformPixelShader = R"HLSL(
cbuffer PixelConstants : register(b0)
{
    float3 u_CameraPosition;
    int    u_PointLightCount;
    float3 u_PointLights_position[16];
    float  u_PointLights_intensity[16];
};

float4 PSMain() : SV_Target
{
    float3 sum = u_CameraPosition;
    for (int i = 0; i < u_PointLightCount && i < 16; ++i)
        sum += u_PointLights_position[i] * u_PointLights_intensity[i];
    return float4(sum * 0.001, 1.0);
}
)HLSL";

TEST(D3D11Smoke, IndexedArrayUniformsWritePackedElements)
{
    auto& env = D3D11DeviceEnv::Get(); (void)env;
    D3D11Shader shader("D3D11SmokeArrays", kArrayUniformVertexShader, kArrayUniformPixelShader);
    ASSERT_NE(shader.GetVertexBlob(), nullptr);

    // Plain member + indexed element names resolve.
    EXPECT_TRUE(shader.HasUniform("u_PointLightCount"));
    EXPECT_TRUE(shader.HasUniform("u_PointLights_position"));
    EXPECT_TRUE(shader.HasUniform("u_PointLights_position[0]"));
    EXPECT_TRUE(shader.HasUniform("u_PointLights_position[15]"));
    EXPECT_FALSE(shader.HasUniform("u_PointLights_position[16]")); // out of range
    EXPECT_FALSE(shader.HasUniform("u_PointLights_position[abc]"));

    // Write element 3 of the position array + element 1 of intensities.
    shader.SetMat4("u_ViewProjection", glm::mat4(1.0f));
    shader.SetMat4("u_Transform", glm::mat4(1.0f));
    shader.SetFloat3("u_CameraPosition", glm::vec3(1.0f, 2.0f, 3.0f));
    shader.SetInt("u_PointLightCount", 4);
    shader.SetFloat3("u_PointLights_position[3]", glm::vec3(0.25f, 0.5f, 0.75f));
    shader.SetFloat ("u_PointLights_intensity[1]", 0.5f);

    // The CPU staging must hold exactly what the next Bind() uploads.
    glm::vec3 pos3(0.0f);
    ASSERT_TRUE(shader.ReadUniformStaging("u_PointLights_position[3]", &pos3, sizeof(pos3)));
    EXPECT_NEAR(pos3.x, 0.25f, 1e-6f);
    EXPECT_NEAR(pos3.y, 0.5f,  1e-6f);
    EXPECT_NEAR(pos3.z, 0.75f, 1e-6f);

    // Neighbor elements stay zero (the write did not smear across the
    // 16-byte packed stride).
    glm::vec3 pos2(9.0f), pos4(9.0f);
    ASSERT_TRUE(shader.ReadUniformStaging("u_PointLights_position[2]", &pos2, sizeof(pos2)));
    ASSERT_TRUE(shader.ReadUniformStaging("u_PointLights_position[4]", &pos4, sizeof(pos4)));
    EXPECT_EQ(pos2, glm::vec3(0.0f));
    EXPECT_EQ(pos4, glm::vec3(0.0f));

    float intensity1 = 0.0f;
    ASSERT_TRUE(shader.ReadUniformStaging("u_PointLights_intensity[1]", &intensity1, sizeof(float)));
    EXPECT_NEAR(intensity1, 0.5f, 1e-6f);
}

// ── Smoke 6: SwapChainTarget FrameBuffer (window default target) ──

TEST(D3D11Smoke, SwapChainTargetFrameBufferHasNoGpuObjects)
{
    auto& env = D3D11DeviceEnv::Get(); (void)env;

    FramebufferSpecification spec;
    spec.SwapChainTarget = true; // 0x0 size, no attachments: legal by contract
    D3D11FrameBuffer framebuffer(spec);

    EXPECT_EQ(framebuffer.GetColorAttachment(), nullptr);
    EXPECT_NO_FATAL_FAILURE(framebuffer.Bind());
    EXPECT_NO_FATAL_FAILURE(framebuffer.Unbind());
    EXPECT_NO_FATAL_FAILURE(framebuffer.Resize(64, 64)); // still no GPU objects
}

// ── Smoke 7: window + swapchain render + CPU readback (stage B core) ──
// K-021 boundary note: the D3D11 swapchain does NOT need the interactive-
// desktop tricks Vulkan does (K-014) - D3D11CreateDevice + CreateSwapChain-
// ForHwnd work in agent sandboxes, including against the WARP rasterizer.
// This test creates a hidden Win32 window, runs the full window flow
// (DirectXGraphicsContext -> RendererAPI adoption -> clear/draw into the
// backbuffer) and reads pixels back from the backbuffer before Present.

#ifdef _WIN32

namespace {

constexpr uint32_t kWinWidth  = 64;
constexpr uint32_t kWinHeight = 64;

// Backend code logs unconditionally (DMGE_LOG_*); without Log::Init the
// spdlog logger is null and every log line segfaults (0xc0000005). The
// D3D11DeviceEnv fixture does this lazily, but the swapchain test must
// run BEFORE any fixture use so it exercises the production device-creation
// path (window context creates the device itself, nothing registered yet).
void EnsureSmokeLogReady()
{
    // Idempotent: the D3D11DeviceEnv fixture may have initialized logging
    // already (Log::Init itself is NOT idempotent - register_logger throws).
    static bool ready = false;
    if (!ready && !Log::GetCoreLogger()) { Log::Init(); }
    ready = true;
}

LRESULT CALLBACK SmokeWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

HWND CreateHiddenSmokeWindow()
{
    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = SmokeWindowProc;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.lpszClassName = "DMGE_D3D11SmokeWindow";
    if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        ADD_FAILURE() << "RegisterClassExA failed, GetLastError=" << GetLastError();
        return nullptr;
    }

    RECT rect{ 0, 0, static_cast<LONG>(kWinWidth), static_cast<LONG>(kWinHeight) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "dmge smoke", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd)
        ADD_FAILURE() << "CreateWindowExA failed, GetLastError=" << GetLastError();
    // Window stays HIDDEN - no message pump needed for D3D11 rendering.
    return hwnd;
}

// Copies the swapchain backbuffer into a CPU-readable staging texture and
// returns the RGBA byte at (x, y). Backbuffer format is B8G8R8A8_UNORM, so
// the byte order in memory is B, G, R, A.
std::array<uint8_t, 4> ReadBackbufferPixel(DirectXGraphicsContext& ctx,
                                           uint32_t x, uint32_t y)
{
    auto* device  = D3D11Backend::Device();
    auto* context = D3D11Backend::Context();
    ID3D11Texture2D* backbuffer = ctx.GetBackbufferTexture();
    EXPECT_NE(device, nullptr);
    EXPECT_NE(context, nullptr);
    EXPECT_NE(backbuffer, nullptr);
    if (!device || !context || !backbuffer)
        return {};

    D3D11_TEXTURE2D_DESC bbDesc{};
    backbuffer->GetDesc(&bbDesc);

    D3D11_TEXTURE2D_DESC stagingDesc = bbDesc;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags      = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MipLevels      = 1;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf());
    EXPECT_HRESULT_SUCCEEDED(hr);
    if (FAILED(hr))
        return {};

    context->CopyResource(staging.Get(), backbuffer);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    EXPECT_HRESULT_SUCCEEDED(hr);
    if (FAILED(hr))
        return {};

    const auto* row = static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch;
    std::array<uint8_t, 4> texel{ row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2], row[x * 4 + 3] };
    context->Unmap(staging.Get(), 0);
    return texel;
}

} // anonymous namespace

TEST(D3D11Smoke, SwapchainWindowRenderReadbackAndResize)
{
    EnsureSmokeLogReady();
    HWND hwnd = CreateHiddenSmokeWindow();
    ASSERT_NE(hwnd, nullptr);

    // The window flow: the context creates (or adopts) the process-wide
    // device and creates the swapchain on it (mirrors Application::
    // Initialize's "window first, Renderer::Init() second" order).
    DirectXGraphicsContext context(hwnd);
    context.Init();
    ASSERT_TRUE(context.IsValid());
    EXPECT_NE(context.GetBackbufferRTV(), nullptr);
    EXPECT_NE(context.GetDepthDSV(), nullptr);
    // The OS may clamp the requested client size (minimum tracking width);
    // the swapchain must exactly match whatever client area the window got.
    const uint32_t winW = context.GetWidth();
    const uint32_t winH = context.GetHeight();
    EXPECT_GT(winW, 0u);
    EXPECT_GT(winH, 0u);
    {
        RECT client{};
        ASSERT_TRUE(GetClientRect(hwnd, &client));
        EXPECT_EQ(winW, static_cast<uint32_t>(client.right - client.left));
        EXPECT_EQ(winH, static_cast<uint32_t>(client.bottom - client.top));
    }

    // A RendererAPI instance for the window flow (adopts the same device;
    // a second hardware device could not present to this swapchain).
    RendererAPIInitConfig config;
    config.ClearColor = glm::vec4(0.1f, 0.2f, 0.3f, 1.0f);
    auto api = DirectX::CreateDirectXRendererAPI();
    api->Init(config);
    auto* d3dApi = static_cast<D3D11RendererAPI*>(api.get());
    EXPECT_STREQ(d3dApi->GetDriverTypeName(), "Windowed");

    // ── Frame 1: clear + triangle into the backbuffer ────────────
    D3D11Shader shader("D3D11SmokeSwapchain", kSmokeVertexShader, kSmokePixelShader);
    ASSERT_NE(shader.GetVertexBlob(), nullptr);

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

    d3dApi->SetViewport(0, 0, static_cast<int>(winW), static_cast<int>(winH));

    // BeginRenderPass(nullptr) = swapchain target: binds the backbuffer RTV
    // + window depth view and clears.
    d3dApi->BeginRenderPass(nullptr);
    shader.Bind();
    shader.SetMat4("u_ViewProjection", glm::mat4(1.0f));
    shader.SetMat4("u_Transform", glm::mat4(1.0f));
    d3dApi->DrawIndexed(vertexArray);
    d3dApi->EndRenderPass();

    // Readback BEFORE Present (flip-model buffers are undefined after it).
    // B8G8R8A8 memory order: B, G, R, A.
    {
        // NDC (0, -0.25) is strictly inside the triangle.
        const uint32_t cx = winW / 2;
        const uint32_t cy = static_cast<uint32_t>((1.0f - (-0.25f * 0.5f + 0.5f)) * winH);
        const auto px = ReadBackbufferPixel(context, cx, cy);
        EXPECT_EQ(px[2], 255) << "triangle interior R";
        EXPECT_LT(px[1], 40)  << "triangle interior G";
        EXPECT_LT(px[0], 40)  << "triangle interior B";
        EXPECT_EQ(px[3], 255) << "triangle interior A";

        const auto corner = ReadBackbufferPixel(context, 2, 2);
        EXPECT_NEAR(corner[2], 26, 3) << "corner R (0.1 * 255)";
        EXPECT_NEAR(corner[1], 51, 3) << "corner G (0.2 * 255)";
        EXPECT_NEAR(corner[0], 77, 3) << "corner B (0.3 * 255)";
    }

    // ── Frame 2: window resize -> ResizeBuffers + re-render ─────
    context.RequestResize(32, 32);
    EXPECT_EQ(context.GetWidth(), 32u);
    EXPECT_EQ(context.GetHeight(), 32u);
    EXPECT_NE(context.GetBackbufferRTV(), nullptr); // recreated

    d3dApi->SetViewport(0, 0, 32, 32);
    d3dApi->BeginRenderPass(nullptr);   // binds + clears to the same color
    d3dApi->EndRenderPass();

    {
        const auto corner = ReadBackbufferPixel(context, 2, 2);
        EXPECT_NEAR(corner[2], 26, 3) << "resized corner R";
        EXPECT_NEAR(corner[1], 51, 3) << "resized corner G";
        EXPECT_NEAR(corner[0], 77, 3) << "resized corner B";
    }

    // SwapBuffers must be callable (vsync Present on a hidden window).
    EXPECT_NO_FATAL_FAILURE(context.SwapBuffers());
}

#endif // _WIN32
