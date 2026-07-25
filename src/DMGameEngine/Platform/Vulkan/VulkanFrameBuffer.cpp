/*
 * DMGameEngine - Vulkan Frame Buffer Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanFrameBuffer.h"

#include "DMGameEngine/Platform/Vulkan/VulkanTextureUtils.h"   // TextureFormatToVk
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

using Detail::TextureFormatToVk;

uint32_t VulkanFrameBuffer::s_IDCounter = 1;

// -- Constructors / Destructor -----------------------------------

VulkanFrameBuffer::VulkanFrameBuffer(const FramebufferSpecification& spec)
    : m_Spec(spec)
{
    Invalidate();
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
    Destroy();
}

// -- Resize ------------------------------------------------------

void VulkanFrameBuffer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    if (width == m_Spec.Width && height == m_Spec.Height)
        return;

    m_Spec.Width  = width;
    m_Spec.Height = height;

    if (m_Spec.SwapChainTarget)
        return;   // default target is owned by the swapchain

    Invalidate();
}

// -- Attachments -------------------------------------------------

DM::Ref<Texture2D> VulkanFrameBuffer::GetColorAttachment(uint32_t index) const
{
    DMGE_CORE_ASSERT(index < m_ColorAttachments.size(),
                     "FrameBuffer color attachment index out of range: {0}", index);
    return m_ColorAttachments[index];
}

VkFormat VulkanFrameBuffer::GetColorFormat(uint32_t index) const
{
    DMGE_CORE_ASSERT(index < m_ColorAttachments.size(),
                     "FrameBuffer color attachment index out of range: {0}", index);
    return TextureFormatToVk(m_ColorAttachments[index]->GetSpecification().Format);
}

VkFormat VulkanFrameBuffer::GetDepthFormat() const
{
    return HasDepth() ? TextureFormatToVk(m_DepthAttachment->GetSpecification().Format)
                      : VK_FORMAT_UNDEFINED;
}

VkImageView VulkanFrameBuffer::GetColorImageView(uint32_t index) const
{
    DMGE_CORE_ASSERT(index < m_ColorAttachments.size(),
                     "FrameBuffer color attachment index out of range: {0}", index);
    return m_ColorAttachments[index]->GetVkImageView();
}

VkImageView VulkanFrameBuffer::GetDepthImageView() const
{
    return HasDepth() ? m_DepthAttachment->GetVkImageView() : VK_NULL_HANDLE;
}

// -- GPU resource (re)creation ------------------------------------

VkImage VulkanFrameBuffer::GetColorImage(uint32_t index) const
{
    DMGE_CORE_ASSERT(index < m_ColorAttachments.size(),
                     "FrameBuffer color attachment index out of range: {0}", index);
    return m_ColorAttachments[index]->GetVkImage();
}

VkImage VulkanFrameBuffer::GetDepthImage() const
{
    return HasDepth() ? m_DepthAttachment->GetVkImage() : VK_NULL_HANDLE;
}
void VulkanFrameBuffer::Invalidate()
{
    Destroy();

    if (m_Spec.SwapChainTarget)
        return;   // default target - no offscreen objects

    // -- Color attachments --------------------------------------
    m_ColorAttachments.clear();
    for (const auto& att : m_Spec.Attachments)
    {
        Texture2DSpecification texSpec;
        texSpec.Width  = m_Spec.Width;
        texSpec.Height = m_Spec.Height;
        texSpec.Format = att.Format;
        texSpec.MinFilter = TextureFilter::Linear;
        texSpec.MagFilter = TextureFilter::Linear;
        texSpec.WrapS     = TextureWrap::ClampToEdge;
        texSpec.WrapT     = TextureWrap::ClampToEdge;
        texSpec.GenerateMipmaps = false;

        // COLOR_ATTACHMENT usage lets a future vkCmdBeginRendering render into
        // this image; SAMPLED (added by VulkanTexture2D) lets the result be
        // sampled afterwards.
        auto tex = DM::CreateRef<VulkanTexture2D>(
            texSpec, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        m_ColorAttachments.push_back(tex);
    }

    // -- Depth / stencil attachment -----------------------------
    if (m_Spec.DepthFormat != TextureFormat::None)
    {
        Texture2DSpecification depthSpec;
        depthSpec.Width  = m_Spec.Width;
        depthSpec.Height = m_Spec.Height;
        depthSpec.Format = m_Spec.DepthFormat;
        depthSpec.MinFilter = TextureFilter::Linear;
        depthSpec.MagFilter = TextureFilter::Linear;
        depthSpec.WrapS     = TextureWrap::ClampToEdge;
        depthSpec.WrapT     = TextureWrap::ClampToEdge;
        depthSpec.GenerateMipmaps = false;

        m_DepthAttachment = DM::CreateRef<VulkanTexture2D>(
            depthSpec, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    m_RendererID = s_IDCounter++;
}

void VulkanFrameBuffer::Destroy()
{
    m_ColorAttachments.clear();   // releases VulkanTexture2D -> destroys image/view/sampler
    m_DepthAttachment.reset();
}

} // namespace DMGameEngine