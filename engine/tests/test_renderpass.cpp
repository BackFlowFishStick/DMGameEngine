/*
 * DMGameEngine - RenderPassDesc pure-logic tests (2b stage 1, E1)
 *
 * Exercises the backend-agnostic render pass description
 * (Renderer/RenderPassDesc.h) headless - no window, no graphics context,
 * no GPU objects:
 *
 *   - Defaults: a freshly constructed desc is the swapchain form
 *     (no target, no attachments) with the OpenGL NdcZMin convention.
 *   - Factories: MakeRenderPassDesc / MakeRenderPassDescForTarget map a
 *     FramebufferSpecification (formats, depth, swapchain flag) onto the
 *     desc, clamp oversized attachment lists, and default load ops to
 *     Clear (the behavior every existing call site was built on).
 *   - Validation: ValidateRenderPassDesc structural checks (count cap,
 *     None formats in used slots, HasDepth <-> Depth.Format pairing,
 *     stale tails allowed per K-024).
 *   - GBufferLayout consistency: MatchesGBufferLayout accepts exactly the
 *     desc the renderer builds for the G-buffer pass and rejects broken
 *     variants (wrong count / format / depth).
 */

#include <gtest/gtest.h>

#include "DMGameEngine/Renderer/RenderPassDesc.h"
#include "DMGameEngine/Renderer/DeferredRendering.h"

using namespace DMGameEngine;

namespace {

// A spec assembled exactly like Renderer::EnsureDeferredResources builds
// the G-buffer FrameBuffer (GBufferLayout as the single source of truth).
FramebufferSpecification MakeGBufferSpec()
{
    FramebufferSpecification spec;
    spec.Width  = 1280;
    spec.Height = 720;
    spec.Attachments.push_back({ GBufferLayout::AlbedoSpecFormat() });
    spec.Attachments.push_back({ GBufferLayout::NormalShininessFormat() });
    spec.DepthFormat = GBufferLayout::DepthFormat();
    return spec;
}

} // namespace

// ── Defaults ───────────────────────────────────────────────────────

TEST(RenderPassDesc, DefaultIsTheSwapchainForm)
{
    RenderPassDesc desc;
    EXPECT_EQ(desc.Target, nullptr);
    EXPECT_EQ(desc.ColorAttachmentCount, 0u);
    EXPECT_FALSE(desc.HasDepth);
    EXPECT_EQ(desc.Depth.Format, TextureFormat::None);
    EXPECT_TRUE(desc.UseRendererClearColor);
    EXPECT_TRUE(desc.UseTargetExtent);
    // OpenGL is the engine's historical convention; backends overwrite the
    // field when the pass begins (GL -1 / Vulkan 0 / DirectX family 0).
    EXPECT_FLOAT_EQ(desc.NdcZMin, -1.0f);
}

TEST(RenderPassDesc, LoadOpsDefaultToClearAndAreDistinct)
{
    RenderPassDesc desc;
    for (uint32_t i = 0; i < RenderPassDesc::kMaxColorAttachments; ++i)
        EXPECT_EQ(desc.Color[i].Load, AttachmentLoadOp::Clear);
    EXPECT_EQ(desc.Depth.Load, AttachmentLoadOp::Clear);
    EXPECT_FLOAT_EQ(desc.Depth.ClearDepth, 1.0f);

    // The three load ops must be distinguishable values.
    EXPECT_NE(static_cast<uint8_t>(AttachmentLoadOp::Load),
              static_cast<uint8_t>(AttachmentLoadOp::Clear));
    EXPECT_NE(static_cast<uint8_t>(AttachmentLoadOp::Clear),
              static_cast<uint8_t>(AttachmentLoadOp::DontCare));
    EXPECT_NE(static_cast<uint8_t>(AttachmentLoadOp::Load),
              static_cast<uint8_t>(AttachmentLoadOp::DontCare));
}

// ── Factories ──────────────────────────────────────────────────────

TEST(RenderPassDesc, NullTargetGivesSwapchainForm)
{
    RenderPassDesc desc = MakeRenderPassDescForTarget(nullptr);
    EXPECT_EQ(desc.Target, nullptr);
    EXPECT_EQ(desc.ColorAttachmentCount, 0u);
    EXPECT_FALSE(desc.HasDepth);
    EXPECT_TRUE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, FromSpecMapsAttachmentsAndDepthInOrder)
{
    FramebufferSpecification spec = MakeGBufferSpec();
    RenderPassDesc desc = MakeRenderPassDesc(spec /* target not needed */);

    EXPECT_EQ(desc.Target, nullptr); // stored verbatim when not provided
    EXPECT_EQ(desc.ColorAttachmentCount, 2u);
    EXPECT_EQ(desc.Color[0].Format, GBufferLayout::AlbedoSpecFormat());
    EXPECT_EQ(desc.Color[1].Format, GBufferLayout::NormalShininessFormat());
    EXPECT_EQ(desc.Color[0].Load, AttachmentLoadOp::Clear);
    EXPECT_EQ(desc.Color[1].Load, AttachmentLoadOp::Clear);
    EXPECT_TRUE(desc.HasDepth);
    EXPECT_EQ(desc.Depth.Format, GBufferLayout::DepthFormat());
    EXPECT_TRUE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, FromSpecWithoutDepthHasNoDepthAttachment)
{
    FramebufferSpecification spec;
    spec.Attachments.push_back({ TextureFormat::RGBA8 });
    spec.DepthFormat = TextureFormat::None;

    RenderPassDesc desc = MakeRenderPassDesc(spec);
    EXPECT_EQ(desc.ColorAttachmentCount, 1u);
    EXPECT_FALSE(desc.HasDepth);
    EXPECT_EQ(desc.Depth.Format, TextureFormat::None);
    EXPECT_TRUE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, FromSpecClampsOversizedAttachmentLists)
{
    FramebufferSpecification spec;
    for (int i = 0; i < 12; ++i)
        spec.Attachments.push_back({ TextureFormat::RGBA8 });

    RenderPassDesc desc = MakeRenderPassDesc(spec);
    EXPECT_EQ(desc.ColorAttachmentCount, RenderPassDesc::kMaxColorAttachments);
    // The last CLAMPED entry carries the 8th attachment; entries beyond the
    // clamp are untouched tail (stale values allowed, K-024 rule) and must
    // not be counted.
    EXPECT_EQ(desc.Color[RenderPassDesc::kMaxColorAttachments - 1].Format, TextureFormat::RGBA8);
    EXPECT_TRUE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, MaxColorAttachmentsHeadroomCoversGBuffer)
{
    EXPECT_GE(RenderPassDesc::kMaxColorAttachments, GBufferLayout::kColorAttachmentCount);
}

// ── Validation ─────────────────────────────────────────────────────

TEST(RenderPassDesc, ValidateAcceptsAWellFormedMrtDesc)
{
    RenderPassDesc desc = MakeRenderPassDesc(MakeGBufferSpec());
    EXPECT_TRUE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, ValidateRejectsCountOverTheCap)
{
    RenderPassDesc desc = MakeRenderPassDesc(MakeGBufferSpec());
    desc.ColorAttachmentCount = RenderPassDesc::kMaxColorAttachments + 1;
    EXPECT_FALSE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, ValidateRejectsNoneFormatInUsedSlot)
{
    RenderPassDesc desc = MakeRenderPassDesc(MakeGBufferSpec());
    desc.Color[0].Format = TextureFormat::None;
    EXPECT_FALSE(ValidateRenderPassDesc(desc));

    // Stale/None TAIL entries beyond the count are allowed (K-024: only
    // the first N entries are meaningful).
    desc = MakeRenderPassDesc(MakeGBufferSpec());
    desc.Color[5].Format = TextureFormat::None;
    EXPECT_TRUE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, ValidateRejectsDepthPairMismatch)
{
    // HasDepth=true but no depth format.
    RenderPassDesc desc = MakeRenderPassDesc(MakeGBufferSpec());
    desc.Depth.Format = TextureFormat::None;
    EXPECT_FALSE(ValidateRenderPassDesc(desc));

    // HasDepth=false but a depth format is set.
    desc = MakeRenderPassDesc(MakeGBufferSpec());
    desc.HasDepth = false;
    EXPECT_FALSE(ValidateRenderPassDesc(desc));
}

TEST(RenderPassDesc, ValidateAcceptsSwapchainForm)
{
    RenderPassDesc desc; // count 0, no depth, no target
    EXPECT_TRUE(ValidateRenderPassDesc(desc));
}

// ── GBufferLayout consistency ──────────────────────────────────────

TEST(RenderPassDesc, GBufferDescMatchesGBufferLayout)
{
    RenderPassDesc desc = MakeRenderPassDesc(MakeGBufferSpec());
    EXPECT_TRUE(MatchesGBufferLayout(desc));
}

TEST(RenderPassDesc, GBufferLayoutMismatchIsDetected)
{
    // Wrong color attachment count.
    {
        FramebufferSpecification spec = MakeGBufferSpec();
        spec.Attachments.pop_back();
        EXPECT_FALSE(MatchesGBufferLayout(MakeRenderPassDesc(spec)));
    }
    // Wrong RT0 format.
    {
        FramebufferSpecification spec = MakeGBufferSpec();
        spec.Attachments[0].Format = TextureFormat::RGBA16F;
        EXPECT_FALSE(MatchesGBufferLayout(MakeRenderPassDesc(spec)));
    }
    // Wrong RT1 format.
    {
        FramebufferSpecification spec = MakeGBufferSpec();
        spec.Attachments[1].Format = TextureFormat::RGBA8;
        EXPECT_FALSE(MatchesGBufferLayout(MakeRenderPassDesc(spec)));
    }
    // Missing depth attachment.
    {
        FramebufferSpecification spec = MakeGBufferSpec();
        spec.DepthFormat = TextureFormat::None;
        EXPECT_FALSE(MatchesGBufferLayout(MakeRenderPassDesc(spec)));
    }
    // Wrong depth format.
    {
        FramebufferSpecification spec = MakeGBufferSpec();
        spec.DepthFormat = TextureFormat::DepthStencil;
        EXPECT_FALSE(MatchesGBufferLayout(MakeRenderPassDesc(spec)));
    }
}
