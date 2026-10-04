/*
 * DMGameEngine - Vulkan Frame Buffer
 *
 * Vulkan implementation of the FrameBuffer abstraction.
 *
 * The engine's Vulkan backend uses VK_KHR_dynamic_rendering, so framebuffer
 * objects (VkFramebuffer) are not strictly required: vkCmdBeginRendering
 * takes VkRenderingAttachmentInfo structs that reference image views
 * directly. VulkanFrameBuffer therefore owns the color/depth attachment
 * images (as VulkanTexture2D, so they remain sampleable) plus their
 * VkImageView / VkFormat accessors, which a future integration will plug into
 * the dynamic-rendering attachment infos.
 *
 * Bind()/Unbind() are no-ops, mirroring the Vulkan backend's per-draw binding
 * philosophy (cf. VulkanVertexBuffer): the active render target is bound at
 * draw-record time inside the command buffer, not through a global Bind() the
 * way OpenGL binds an FBO. They exist for API symmetry with the OpenGL
 * backend and the cross-API FrameBuffer interface.
 */

#pragma once

#include "DMGameEngine/Renderer/FrameBuffer.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTexture2D.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace DMGameEngine {

class DMGE_API VulkanFrameBuffer : public FrameBuffer
{
public:
    explicit VulkanFrameBuffer(const FramebufferSpecification& spec);
    ~VulkanFrameBuffer() override;

    void Bind()   override {}   // see class doc: dynamic rendering binds per-draw
    void Unbind() override {}

    void Resize(uint32_t width, uint32_t height) override;

    uint32_t GetWidth()      const override { return m_Spec.Width;  }
    uint32_t GetHeight()     const override { return m_Spec.Height; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    DM::Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const override;
    size_t             GetColorAttachmentCount()        const override { return m_ColorAttachments.size(); }
    DM::Ref<Texture2D> GetDepthAttachment()             const override { return m_DepthAttachment; }

    const FramebufferSpecification& GetSpecification() const override { return m_Spec; }

    // -- Vulkan interop (for future dynamic-rendering integration) --
    // These expose what vkCmdBeginRendering's VkRenderingAttachmentInfo /
    // VkPipelineRenderingCreateInfo need: the attachment image views and
    // their formats.
    VkFormat    GetColorFormat(uint32_t index = 0) const;
    VkFormat    GetDepthFormat() const;
    VkImageView GetColorImageView(uint32_t index = 0) const;
    VkImageView GetDepthImageView() const;
    VkImage    GetColorImage(uint32_t index = 0) const;
    VkImage    GetDepthImage() const;
    bool        HasDepth() const { return m_DepthAttachment != nullptr; }

private:
    void Invalidate();   // (re)create the attachments from m_Spec
    void Destroy();

    FramebufferSpecification m_Spec;
    std::vector<DM::Ref<VulkanTexture2D>> m_ColorAttachments;
    DM::Ref<VulkanTexture2D>               m_DepthAttachment;

    uint32_t m_RendererID = 0;   // opaque id for debug / cross-backend parity
    static uint32_t s_IDCounter;
};

} // namespace DMGameEngine