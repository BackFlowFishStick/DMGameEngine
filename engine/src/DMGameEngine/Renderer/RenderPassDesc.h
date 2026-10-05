/*
 * DMGameEngine - Render Pass Description (2b stage 1, ENGINE_REVIEW E1)
 *
 * Backend-agnostic DESCRIPTION of a single render pass: which attachments
 * are written (format + load op + clear values), the depth attachment, the
 * viewport/scissor region and the target FrameBuffer association. This is
 * pure description data - it holds NO GPU objects and no backend enums
 * (R5): each backend's BeginRenderPass consumes it in its own terms
 * (OpenGL: FBO bind + glClear; Vulkan: dynamic rendering attachment infos
 * + layout transitions; DirectX: OM bind + ClearRenderTargetView).
 *
 * Migration shape (stage 1): RendererAPI::BeginRenderPass(FrameBuffer*) is
 * KEPT for call-site compatibility; the backends route it through the desc
 * form (RenderPassDescForTarget) so there is exactly ONE consumption path
 * per backend. New code may pass a RenderPassDesc directly. Everything in
 * this header is inline/pure so it unit-tests headless (see
 * engine/tests/test_renderpass.cpp).
 *
 * Debts absorbed here (documents/DEFERRED_RENDERING_DESIGN.md §7):
 *   - u_NdcZMin: NdcZMin is a desc field annotated by the backend when the
 *     pass begins (OpenGL -1 / Vulkan 0 / DirectX-family 0), read back via
 *     RenderCommand::GetActiveRenderPassDesc() - the Renderer layer no
 *     longer branches on the API.
 *   - K-024: the Vulkan pipeline key's colorAttachmentCount + active color
 *     formats are now fed from the desc's ColorAttachmentCount/Format
 *     entries, so a pass's structure is described once and flows into the
 *     pipeline cache key unchanged.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Texture.h"      // TextureFormat
#include "DMGameEngine/Renderer/FrameBuffer.h"  // FrameBuffer / FramebufferSpecification

#include <algorithm>
#include <array>
#include <cstdint>

namespace DMGameEngine {

// ── Attachment load op ─────────────────────────────────────────────
// What happens to an attachment's prior CONTENTS when the pass begins.
// Maps 1:1 onto VkAttachmentLoadOp; OpenGL realizes only the Load/Clear
// distinction (a glClear of the freshly-bound FBO vs leaving it).
enum class AttachmentLoadOp : uint8_t
{
    Load     = 0, // preserve prior contents (Vulkan LOAD; GL: no clear)
    Clear    = 1, // clear to the desc's clear value (Vulkan CLEAR; GL: glClear)
    DontCare = 2, // contents undefined (Vulkan DONT_CARE; GL treated as no clear)
};

// Engine-side headroom for MRT. The deferred G-buffer uses 2. The Vulkan
// backend caps at its own kMaxColorAttachments (4) with an assert; the GL
// side is bounded by the FBO's glDrawBuffers table (K-024: draw buffers are
// FBO state, so GL needs no per-pass pipeline identity for them).
inline constexpr uint32_t kMaxRenderPassColorAttachments = 8;

// ── Color attachment entry ─────────────────────────────────────────
struct DMGE_API RenderPassColorAttachment
{
    TextureFormat    Format = TextureFormat::RGBA8;
    AttachmentLoadOp Load   = AttachmentLoadOp::Clear;
};

// ── Depth/stencil attachment entry ─────────────────────────────────
struct DMGE_API RenderPassDepthAttachment
{
    TextureFormat    Format       = TextureFormat::None;
    AttachmentLoadOp Load         = AttachmentLoadOp::Clear;
    float            ClearDepth   = 1.0f;
    uint32_t         ClearStencil = 0;
};

// ── Render pass description ────────────────────────────────────────
// Trivially copyable aggregate (std::array + PODs only - R1: no dynamic
// containers on an exported struct; the fixed array is the sanctioned
// static-array form).
struct DMGE_API RenderPassDesc
{
    static constexpr uint32_t kMaxColorAttachments = kMaxRenderPassColorAttachments;

    // Target FrameBuffer (non-owning raw pointer). nullptr or a
    // SwapChainTarget FrameBuffer means the default/swapchain target, whose
    // attachment layout the backend knows itself - in that form the desc's
    // attachment fields are advisory and may be left at the swapchain-form
    // defaults (count 0, no depth).
    FrameBuffer* Target = nullptr;

    // Color attachments. Only the first ColorAttachmentCount entries are
    // meaningful; tail entries may hold stale values (K-024 rule: hash /
    // validate / consume only the first N).
    std::array<RenderPassColorAttachment, kMaxColorAttachments> Color{};
    uint32_t ColorAttachmentCount = 0;

    // Depth/stencil attachment. HasDepth must agree with Depth.Format
    // (ValidateRenderPassDesc checks the pair).
    RenderPassDepthAttachment Depth{};
    bool HasDepth = false;

    // Clear color source. When true (default - preserves the pre-desc
    // behavior) the backend clears with the renderer-wide clear color
    // (RendererAPIInitConfig::ClearColor / SetClearColor); when false the
    // desc's ClearColor is used. Per-attachment clear colors are a 2b
    // follow-up (GL would need glClearBufferfv per draw buffer).
    // Plain float array, NOT glm::vec4: this struct is DMGE_API and a glm
    // member would trip C4251 at every instantiation (glm has no
    // dll-interface; std::array of PODs does not).
    bool                UseRendererClearColor = true;
    std::array<float, 4> ClearColor            { 0.0f, 0.0f, 0.0f, 0.0f };

    // Viewport/scissor. When UseTargetExtent is true (default - preserves
    // the pre-desc behavior) each backend uses the target's full extent;
    // when false, the explicit values below are applied at pass begin.
    bool  UseTargetExtent = true;
    float ViewportX = 0.0f, ViewportY = 0.0f, ViewportWidth = 0.0f, ViewportHeight = 0.0f;
    float ScissorX  = 0.0f, ScissorY  = 0.0f, ScissorWidth  = 0.0f, ScissorHeight = 0.0f;

    // NDC z minimum of the depth range under the ACTIVE backend's clip
    // convention: OpenGL maps NDC z [-1,1] -> window [0,1] (-1.0), Vulkan
    // and the DirectX family use NDC z [0,1] (0.0). The field defaults to
    // the OpenGL value (the engine's historical convention); the backend
    // OVERWRITES it in its own snapshot when the pass begins. Consumers
    // (deferred lighting depth reprojection) read it back via
    // RenderCommand::GetActiveRenderPassDesc() - no Renderer-layer API
    // branch (E1 leak removed; DEFERRED_RENDERING_DESIGN.md §7).
    float NdcZMin = -1.0f;
};

// ── Factories ──────────────────────────────────────────────────────
// Describe a pass from a FrameBuffer's specification (no GPU objects
// touched - works headless). target is stored verbatim (may be null).
// Attachment count is clamped to kMaxColorAttachments; load ops default to
// Clear (the behavior every existing call site was built on).
inline RenderPassDesc MakeRenderPassDesc(const FramebufferSpecification& spec,
                                         FrameBuffer* target = nullptr)
{
    RenderPassDesc desc;
    desc.Target = target;
    const uint32_t count = static_cast<uint32_t>(spec.Attachments.size());
    desc.ColorAttachmentCount = std::min(count, RenderPassDesc::kMaxColorAttachments);
    for (uint32_t i = 0; i < desc.ColorAttachmentCount; ++i)
    {
        desc.Color[i].Format = spec.Attachments[i].Format;
        desc.Color[i].Load   = AttachmentLoadOp::Clear;
    }
    desc.HasDepth     = spec.DepthFormat != TextureFormat::None;
    desc.Depth.Format = desc.HasDepth ? spec.DepthFormat : TextureFormat::None;
    desc.Depth.Load   = AttachmentLoadOp::Clear;
    return desc;
}

// Convenience: desc for a (possibly null) FrameBuffer target. nullptr and
// SwapChainTarget FrameBuffers yield the swapchain form (count 0, no depth
// - the backend knows the swapchain's own layout).
inline RenderPassDesc MakeRenderPassDescForTarget(FrameBuffer* target)
{
    if (!target || target->GetSpecification().SwapChainTarget)
        return RenderPassDesc{}; // swapchain form (Target stays null)
    return MakeRenderPassDesc(target->GetSpecification(), target);
}

// ── Validation (pure logic, unit-tested) ───────────────────────────
// Structural self-consistency of a desc. Swapchain-form descs (no target)
// are always valid - the backend supplies the real attachment layout.
inline bool ValidateRenderPassDesc(const RenderPassDesc& desc)
{
    if (desc.ColorAttachmentCount > RenderPassDesc::kMaxColorAttachments)
        return false;
    // Only the first N entries are meaningful; stale/None tails are allowed
    // (K-024 rule).
    for (uint32_t i = 0; i < desc.ColorAttachmentCount; ++i)
    {
        if (desc.Color[i].Format == TextureFormat::None)
            return false;
    }
    // HasDepth and Depth.Format must agree.
    if (desc.HasDepth != (desc.Depth.Format != TextureFormat::None))
        return false;
    return true;
}

} // namespace DMGameEngine
