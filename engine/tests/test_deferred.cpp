/*
 * DMGameEngine - deferred rendering pure-logic tests (3e stage 1)
 *
 * Exercises the backend-agnostic parts of the configurable Forward/Deferred
 * render path (Renderer/DeferredRendering.h) headless - no window, no
 * graphics context, no GPU objects (kb/KB-07 K-021):
 *
 *   - GBufferLayout: the G-buffer attachment/format table invariants that
 *     the shaders, the renderer orchestration and the lighting pass sampler
 *     slots all depend on.
 *   - DeferredPathState: the request/active snapshot pair (path switches
 *     take effect at BeginScene, never mid-frame; Forward is the default).
 *   - ShouldRebuildGBuffer: resize-vs-reuse decision for the G-buffer
 *     FrameBuffer (zero / absurd sizes never build; rebuild only on change).
 *   - Fullscreen triangle geometry: covers the viewport, linear UVs.
 *   - Embedded shader sources: non-empty, MRT output declarations match
 *     GBufferLayout's color attachment count.
 */

#include <gtest/gtest.h>

#include "DMGameEngine/Renderer/DeferredRendering.h"

using namespace DMGameEngine;

// ── RenderPath defaults ────────────────────────────────────────────

TEST(DeferredPathState, ForwardIsTheDefaultPath)
{
    DeferredPathState state;
    EXPECT_EQ(state.Requested, RenderPath::Forward);
    EXPECT_EQ(state.Active, RenderPath::Forward);
    EXPECT_FALSE(state.IsDeferredFrame());
}

TEST(DeferredPathState, SwitchTakesEffectAtNextBeginSceneNotMidFrame)
{
    DeferredPathState state;

    // A switch requested mid-frame (after BeginScene) must not change the
    // frame's active path.
    state.OnBeginScene();               // frame starts as Forward
    state.Request(RenderPath::Deferred);
    EXPECT_EQ(state.Active, RenderPath::Forward);   // current frame unaffected
    EXPECT_FALSE(state.IsDeferredFrame());

    state.OnBeginScene();               // next frame picks it up
    EXPECT_TRUE(state.IsDeferredFrame());

    // ... and switching back behaves the same way.
    state.Request(RenderPath::Forward);
    EXPECT_TRUE(state.IsDeferredFrame());           // still deferred this frame
    state.OnBeginScene();
    EXPECT_FALSE(state.IsDeferredFrame());
}

TEST(DeferredPathState, RepeatedRequestsAreIdempotent)
{
    DeferredPathState state;
    state.Request(RenderPath::Deferred);
    state.Request(RenderPath::Deferred);
    state.OnBeginScene();
    EXPECT_TRUE(state.IsDeferredFrame());
}

// ── G-buffer layout table ──────────────────────────────────────────

TEST(GBufferLayout, AttachmentTableMatchesDesignDoc)
{
    // Design doc section 3: RT0 RGBA8 albedo+spec, RT1 RGBA16F
    // normal+shininess, depth attachment reused for reprojection.
    EXPECT_EQ(GBufferLayout::kColorAttachmentCount, 2u);
    EXPECT_EQ(GBufferLayout::AlbedoSpecFormat(), TextureFormat::RGBA8);
    EXPECT_EQ(GBufferLayout::NormalShininessFormat(), TextureFormat::RGBA16F);
    EXPECT_EQ(GBufferLayout::DepthFormat(), TextureFormat::Depth);
}

TEST(GBufferLayout, SamplerSlotsAreDistinctAndEnumerated)
{
    // Lighting pass binds RT0, RT1 and depth as three distinct texture slots.
    EXPECT_EQ(GBufferLayout::kAlbedoSpecSlot, 0u);
    EXPECT_EQ(GBufferLayout::kNormalShininessSlot, 1u);
    EXPECT_EQ(GBufferLayout::kDepthSlot, 2u);
    EXPECT_NE(GBufferLayout::kAlbedoSpecSampler, GBufferLayout::kNormalShininessSampler);
    EXPECT_NE(GBufferLayout::kNormalShininessSampler, GBufferLayout::kDepthSampler);
}

TEST(GBufferLayout, FormatsAreColorRenderableForMrt)
{
    // RGBA8 / RGBA16F are the color-attachment workhoses on both backends;
    // the depth format must never appear as a color attachment.
    EXPECT_NE(GBufferLayout::AlbedoSpecFormat(), TextureFormat::Depth);
    EXPECT_NE(GBufferLayout::NormalShininessFormat(), TextureFormat::Depth);
    EXPECT_NE(GBufferLayout::AlbedoSpecFormat(), TextureFormat::None);
    EXPECT_NE(GBufferLayout::NormalShininessFormat(), TextureFormat::None);
}

// ── G-buffer rebuild decision ──────────────────────────────────────

TEST(ShouldRebuildGBuffer, RebuildsOnlyOnDimensionChange)
{
    EXPECT_FALSE(ShouldRebuildGBuffer(1280, 720, 1280, 720));
    EXPECT_TRUE(ShouldRebuildGBuffer(1280, 720, 1920, 1080));
    EXPECT_TRUE(ShouldRebuildGBuffer(1280, 720, 1280, 1080));
    EXPECT_TRUE(ShouldRebuildGBuffer(1280, 720, 1920, 720));
}

TEST(ShouldRebuildGBuffer, NeverBuildsForZeroSizes)
{
    // No target size yet (e.g. swapchain target before the first resize):
    // keep whatever exists instead of building a 0-sized framebuffer.
    EXPECT_FALSE(ShouldRebuildGBuffer(0, 0, 0, 0));
    EXPECT_FALSE(ShouldRebuildGBuffer(1280, 720, 0, 720));
    EXPECT_FALSE(ShouldRebuildGBuffer(1280, 720, 1280, 0));
}

TEST(ShouldRebuildGBuffer, RejectsAbsurdSizes)
{
    // Parity with the GL FBO K-017 guard: a negative-float cast upstream
    // becomes a huge uint32 - reject instead of building garbage GPU state.
    EXPECT_FALSE(ShouldRebuildGBuffer(1280, 720, 0xFFFFFFFFu, 720));
    EXPECT_FALSE(ShouldRebuildGBuffer(1280, 720, 1280, 0xFFFFFFFFu));
    EXPECT_FALSE(ShouldRebuildGBuffer(1280, 720, 16385, 720));
    EXPECT_TRUE(ShouldRebuildGBuffer(1280, 720, 16384, 720));   // boundary ok
}

// ── Full-screen triangle ───────────────────────────────────────────

TEST(FullscreenTriangle, CoversTheViewportWithLinearUvs)
{
    // Three vertices, NDC positions, no index buffer.
    for (const auto& v : kFullscreenTri)
    {
        EXPECT_EQ(v.Position.z, 0.0f);
        EXPECT_GE(v.TexCoords.x, 0.0f);
        EXPECT_GE(v.TexCoords.y, 0.0f);
    }
    // The three corners must span beyond [0,1]² so interpolation stays
    // linear over the whole screen (standard full-screen triangle trick).
    EXPECT_FLOAT_EQ(kFullscreenTri[0].Position.x, -1.0f);
    EXPECT_FLOAT_EQ(kFullscreenTri[0].Position.y, -1.0f);
    EXPECT_FLOAT_EQ(kFullscreenTri[1].Position.x, 3.0f);
    EXPECT_FLOAT_EQ(kFullscreenTri[2].Position.y, 3.0f);
    // UV mapping is 1:1 with the vertices (uv = pos*0.5 + 0.5 per axis).
    for (const auto& v : kFullscreenTri)
    {
        EXPECT_FLOAT_EQ(v.TexCoords.x, v.Position.x * 0.5f + 0.5f);
        EXPECT_FLOAT_EQ(v.TexCoords.y, v.Position.y * 0.5f + 0.5f);
    }
}

// ── Embedded shader sources ────────────────────────────────────────

namespace {

uint32_t CountMrtOutputs(const char* source)
{
    // Count `layout(location = N) out vec4` fragment outputs - must match
    // GBufferLayout::kColorAttachmentCount.
    std::string src(source);
    uint32_t count = 0;
    for (size_t pos = src.find("layout(location"); pos != std::string::npos;
         pos = src.find("layout(location", pos + 1))
    {
        size_t outPos = src.find("out vec4", pos);
        if (outPos != std::string::npos && outPos - pos < 40)
            ++count;
    }
    return count;
}

} // namespace

TEST(DeferredShaderSources, GBufferFragmentOutputsMatchAttachmentCount)
{
    EXPECT_EQ(CountMrtOutputs(DeferredShaderSources::GBufferFS()),
              GBufferLayout::kColorAttachmentCount);
    EXPECT_EQ(CountMrtOutputs(DeferredShaderSources::GBufferSkinnedFS()),
              GBufferLayout::kColorAttachmentCount);
    EXPECT_EQ(CountMrtOutputs(DeferredShaderSources::GBufferInstancedFS()),
              GBufferLayout::kColorAttachmentCount);
    // The lighting pass writes a single full-screen color output.
    EXPECT_EQ(CountMrtOutputs(DeferredShaderSources::DeferredLightingFS()), 1u);
}

TEST(DeferredShaderSources, AllStagesAreNonEmpty)
{
    EXPECT_STRNE(DeferredShaderSources::GBufferVS(), "");
    EXPECT_STRNE(DeferredShaderSources::GBufferFS(), "");
    EXPECT_STRNE(DeferredShaderSources::GBufferSkinnedVS(), "");
    EXPECT_STRNE(DeferredShaderSources::GBufferSkinnedFS(), "");
    EXPECT_STRNE(DeferredShaderSources::GBufferInstancedVS(), "");
    EXPECT_STRNE(DeferredShaderSources::GBufferInstancedFS(), "");
    EXPECT_STRNE(DeferredShaderSources::DeferredLightingVS(), "");
    EXPECT_STRNE(DeferredShaderSources::DeferredLightingFS(), "");
}

TEST(DeferredShaderSources, LightingShaderSamplesAllThreeAttachments)
{
    std::string fs(DeferredShaderSources::DeferredLightingFS());
    EXPECT_NE(fs.find(GBufferLayout::kAlbedoSpecSampler), std::string::npos);
    EXPECT_NE(fs.find(GBufferLayout::kNormalShininessSampler), std::string::npos);
    EXPECT_NE(fs.find(GBufferLayout::kDepthSampler), std::string::npos);
    // Depth reprojection inputs the Renderer uploads per frame.
    EXPECT_NE(fs.find("u_InverseViewProjection"), std::string::npos);
    EXPECT_NE(fs.find("u_NdcZMin"), std::string::npos);
    // Light uniform names must stay identical to the forward path
    // (UploadSceneLighting is shared) - spot-check the family prefixes.
    EXPECT_NE(fs.find("u_PointLights_"), std::string::npos);
    EXPECT_NE(fs.find("u_SpotLights_"), std::string::npos);
    EXPECT_NE(fs.find("u_DirectionalLight_"), std::string::npos);
}
