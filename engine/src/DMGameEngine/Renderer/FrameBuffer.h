/*
 * DMGameEngine - Frame Buffer Abstraction
 *
 * Base class for all graphics API framebuffer (render-target) implementations.
 *
 * A FrameBuffer is the OUTPUT surface a draw writes pixels into: it bundles one
 * or more color attachments (textures the scene is rendered into) plus an
 * optional depth/stencil attachment. This is the conceptual opposite of
 * VertexBuffer / IndexBuffer, which feed geometry data INTO the pipeline. Its
 * color attachments are exposed as Texture2D so they can be sampled afterwards
 * (render-to-texture: post-processing, shadow maps, editor viewports,
 * reflections, picking, ...).
 *
 * Platform backends (OpenGL, Vulkan) derive from this and provide their own
 * attachment creation, binding and resizing. FrameBuffers are created via the
 * static Create() factory, which selects the correct backend based on the
 * active Renderer::API.
 *
 * NOTE (scope): this abstraction is self-contained and does not yet rewire the
 * frame lifecycle. Rendering currently always targets the swapchain / default
 * framebuffer; plumbing an offscreen FrameBuffer into the renderer pass model
 * (BeginScene target / dynamic-rendering attachments) is a follow-up. The
 * Vulkan backend therefore exposes the VkImageView / VkFormat accessors needed
 * by that future integration.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Texture.h"     // TextureFormat
#include "DMGameEngine/Renderer/Texture2D.h"    // DM::Ref<Texture2D>

#include <cstdint>
#include <vector>

namespace DMGameEngine {

// -- Attachment Description ------------------------------------------
// Describes a single color attachment's texture format. Multiple color
// attachments enable Multiple-Render-Targets (MRT), e.g. G-buffers.
struct DMGE_API FramebufferTextureAttachment
{
    TextureFormat Format = TextureFormat::RGBA8;
};

// -- Framebuffer Specification ---------------------------------------
struct DMGE_API FramebufferSpecification
{
    uint32_t Width  = 0;
    uint32_t Height = 0;

    // Color attachments (render targets). May be empty for a depth-only
    // target (e.g. a shadow map).
    std::vector<FramebufferTextureAttachment> Attachments;

    // Depth/stencil attachment format. Set to TextureFormat::None to
    // disable depth entirely.
    TextureFormat DepthFormat = TextureFormat::Depth;

    // MSAA sample count (1 = no multisampling). Currently informational;
    // backends allocate 1-sample storage.
    uint32_t Samples = 1;

    // When true the framebuffer represents the default render target
    // (OpenGL framebuffer 0 / the Vulkan swapchain) instead of an offscreen
    // set of textures. Bind() binds the default target and no GPU objects
    // are created; Attachments / DepthFormat are ignored.
    bool SwapChainTarget = false;
};

// -- Frame Buffer ---------------------------------------------------
class DMGE_API FrameBuffer
{
public:
    virtual ~FrameBuffer() = default;

    // Binds this target for rendering. The previously-bound target is saved
    // so Unbind() restores it, allowing scene layers to render into their
    // own offscreen targets without clobbering each other.
    virtual void Bind()   = 0;
    virtual void Unbind() = 0;

    // Re-creates the attachments at a new size (drops previous contents).
    virtual void Resize(uint32_t width, uint32_t height) = 0;

    virtual uint32_t GetWidth()      const = 0;
    virtual uint32_t GetHeight()     const = 0;
    virtual uint32_t GetRendererID() const = 0;

    // Sampleable color attachment produced by rendering into this target.
    virtual DM::Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const = 0;
    virtual size_t             GetColorAttachmentCount()               const = 0;

    // Sampleable depth attachment (deferred lighting samples it to reproject
    // world positions; also shadow-map use cases). Default nullptr for
    // backends/targets without a depth attachment.
    virtual DM::Ref<Texture2D> GetDepthAttachment() const { return nullptr; }

    virtual const FramebufferSpecification& GetSpecification() const = 0;

    // -- Factory --------------------------------------------------
    static DM::Ref<FrameBuffer> Create(const FramebufferSpecification& spec);
};

} // namespace DMGameEngine