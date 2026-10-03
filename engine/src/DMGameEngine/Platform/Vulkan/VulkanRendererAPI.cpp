/*
 * DMGameEngine - Vulkan Renderer API Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"
#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanShader.h"
#include "DMGameEngine/Platform/Vulkan/VulkanVertexArray.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTexture.h"
#include "DMGameEngine/Platform/Vulkan/VulkanFrameBuffer.h"

#include "DMGameEngine/Renderer/VertexArray.h"

#include "DMGameEngine/Core/Log.h"

#include <cstring>
#include <functional>

namespace DMGameEngine {

VulkanRendererAPI* VulkanRendererAPI::s_Instance = nullptr;

namespace {

// ── Enum converters (engine -> Vulkan) ───────────────────────────

VkBlendFactor BlendFactorToVk(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::Zero:                  return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:                   return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case BlendFactor::ConstantAlpha:         return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case BlendFactor::OneMinusConstantAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendFactor!");
    return VK_BLEND_FACTOR_ZERO;
}

VkBlendOp BlendEquationToVk(BlendEquation e)
{
    switch (e)
    {
        case BlendEquation::Add:             return VK_BLEND_OP_ADD;
        case BlendEquation::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BlendEquation::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendEquation::Min:             return VK_BLEND_OP_MIN;
        case BlendEquation::Max:             return VK_BLEND_OP_MAX;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendEquation!");
    return VK_BLEND_OP_ADD;
}

VkCompareOp DepthFuncToVk(DepthFunc f)
{
    switch (f)
    {
        case DepthFunc::Never:         return VK_COMPARE_OP_NEVER;
        case DepthFunc::Less:          return VK_COMPARE_OP_LESS;
        case DepthFunc::Equal:         return VK_COMPARE_OP_EQUAL;
        case DepthFunc::LessEqual:     return VK_COMPARE_OP_LESS_OR_EQUAL;
        case DepthFunc::Greater:       return VK_COMPARE_OP_GREATER;
        case DepthFunc::NotEqual:      return VK_COMPARE_OP_NOT_EQUAL;
        case DepthFunc::GreaterEqual:  return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case DepthFunc::Always:        return VK_COMPARE_OP_ALWAYS;
    }
    DMGE_CORE_ASSERT(false, "Unknown DepthFunc!");
    return VK_COMPARE_OP_LESS;
}

VkCullModeFlags CullModeToVk(CullMode m)
{
    switch (m)
    {
        case CullMode::None:         return VK_CULL_MODE_NONE;
        case CullMode::Front:        return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:         return VK_CULL_MODE_BACK_BIT;
        case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    DMGE_CORE_ASSERT(false, "Unknown CullMode!");
    return VK_CULL_MODE_NONE;
}

VkDeviceSize AlignUp(VkDeviceSize v, VkDeviceSize a)
{
    return (v + a - 1) & ~(a - 1);
}

// ── Descriptor-set cache (P0-1) ──────────────────────────────────────
//
// Key = (shaderID, hash of the bound texture handles). All descriptor
// content is covered by this key:
//   - binding 0 (dynamic UBO): buffer handle + range are constant within
//     a frame (the per-frame scratch buffer); the per-draw variation is
//     the dynamic offset, applied at vkCmdBindDescriptorSets, not stored
//     in the set.
//   - bindings 1..N (combined image samplers): the exact sampler + view
//     handles that WriteDescriptorSet writes, with the global 1x1 dummy
//     substituted for unbound slots (deterministic).
//   - the descriptor set layout itself is owned by the VulkanShader
//     instance; shaderID identifies that instance uniquely (monotonic,
//     never reused), so a key can never alias another shader's layout.
//
// Lifetime: one map per frame-in-flight slot, cleared together with the
// descriptor pool in ResetFrame (pool reset invalidates every set allocated
// from it). Storage is file-static (R1: no STL members on an exported
// class; same pattern as VulkanDevice's deletion buckets).
struct DescKey
{
    uint64_t shaderID = 0;
    uint64_t texHash  = 0;
    bool operator==(const DescKey& o) const { return shaderID == o.shaderID && texHash == o.texHash; }
};
struct DescKeyHash
{
    size_t operator()(const DescKey& k) const noexcept
    {
        size_t h = std::hash<uint64_t>{}(k.shaderID);
        h ^= std::hash<uint64_t>{}(k.texHash) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// Must match VulkanRendererAPI::kMaxFramesInFlight.
constexpr uint32_t kDescCacheFrameSlots = 2;

std::unordered_map<DescKey, VkDescriptorSet, DescKeyHash>&
DescriptorCache(uint32_t frameIndex)
{
    static std::unordered_map<DescKey, VkDescriptorSet, DescKeyHash>
        s_Cache[kDescCacheFrameSlots];
    return s_Cache[frameIndex % kDescCacheFrameSlots];
}

void PurgeShaderFromDescriptorCache(uint64_t shaderID)
{
    for (uint32_t i = 0; i < kDescCacheFrameSlots; ++i)
    {
        auto& cache = DescriptorCache(i);
        for (auto it = cache.begin(); it != cache.end();)
        {
            if (it->first.shaderID == shaderID)
                it = cache.erase(it);
            else
                ++it;
        }
    }
}

} // anonymous namespace

// ── Lifetime ──────────────────────────────────────────────────────

VulkanRendererAPI::~VulkanRendererAPI()
{
    auto& dev = VulkanDevice::Get();
    vkDeviceWaitIdle(dev.Device);

    for (auto& [key, pipeline] : m_Pipelines)
        vkDestroyPipeline(dev.Device, pipeline, nullptr);
    m_Pipelines.clear();

    if (m_PipelineCache != VK_NULL_HANDLE)
        vkDestroyPipelineCache(dev.Device, m_PipelineCache, nullptr);

    DestroyDummyResources();
    DestroyFrameResources();

    if (s_Instance == this)
        s_Instance = nullptr;
}

void VulkanRendererAPI::Init(const RendererAPIInitConfig& config)
{
    s_Instance = this;

    // Apply the requested initial pipeline state through the virtual
    // setters (same as the base class, kept here for clarity / override).
    RendererAPI::Init(config);

    auto& dev = VulkanDevice::Get();

    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK(vkCreatePipelineCache(dev.Device, &cacheInfo, nullptr, &m_PipelineCache));

    CreateFrameResources();
    CreateDummyResources();

    // A previous renderer-API instance (runtime SetAPI switch) may have left
    // entries whose descriptor pools no longer exist; start clean.
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
        DescriptorCache(i).clear();

    DMGE_LOG_INFO("Vulkan renderer API initialized.");
}

void VulkanRendererAPI::OnSwapchainRecreate()
{
    auto& dev = VulkanDevice::Get();

    // Destroy all cached pipelines. The next Clear()/BeginRenderPass will
    // refresh m_ActiveColorFormat/m_ActiveDepthFormat from the new swapchain
    // (or offscreen target), so pipelines are recreated lazily with the
    // correct format on their next draw. The VkPipelineCache object is kept.
    for (auto& [key, pipeline] : m_Pipelines)
    {
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(dev.Device, pipeline, nullptr);
    }
    m_Pipelines.clear();
}

// ── Frame / clear / viewport ──────────────────────────────────────

void VulkanRendererAPI::Clear()
{
    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
    {
        ctx.BeginFrame(m_ClearColor);
        ResetFrame(ctx.GetCurrentFrame());
    }
    // The swapchain pass opened by BeginFrame is the active render target;
    // record its formats as the baseline for PipelineKey (overridden by an
    // offscreen BeginRenderPass).
    m_ActiveColorFormat = ctx.GetSwapchain().GetImageFormat();
    m_ActiveDepthFormat = ctx.GetSwapchain().GetDepthFormat();
    // If a frame is already started, the framebuffer was cleared at
    // BeginFrame (loadOp = CLEAR); nothing more to do.
}

void VulkanRendererAPI::BeginRenderPass(FrameBuffer* target)
{
    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
        return; // no frame yet: nothing to bind (defensive)

    VkCommandBuffer cmd = ctx.GetCurrentCommandBuffer();

    // Default / swapchain target: the swapchain pass is already open from
    // BeginFrame (Clear). Just record its formats as the active pipeline
    // formats and leave the pass open.
    const bool offscreen = (target != nullptr) && !target->GetSpecification().SwapChainTarget;
    if (!offscreen)
    {
        auto& swap = ctx.GetSwapchain();
        m_ActiveColorFormat = swap.GetImageFormat();
        m_ActiveDepthFormat = swap.GetDepthFormat();
        m_ActiveFrameBuffer = nullptr;
        return;
    }

    auto* fbo = static_cast<VulkanFrameBuffer*>(target);
    DMGE_CORE_ASSERT(fbo->GetColorAttachmentCount() > 0,
                     "Vulkan: offscreen FrameBuffer has no color attachment (depth-only RT not supported yet).");
    m_ActiveFrameBuffer = fbo;

    // End the swapchain dynamic-rendering pass BeginFrame opened so we can
    // begin one targeting the FBO (vkCmdBeginRendering cannot nest).
    vkCmdEndRendering(cmd);

    // Transition FBO color/depth images UNDEFINED -> attachment layout. UNDEFINED
    // discards prior contents (we clear), so this is valid regardless of layout.
    for (uint32_t i = 0; i < fbo->GetColorAttachmentCount(); ++i)
    {
        VulkanGraphicsContext::TransitionImageLayout(
            cmd, fbo->GetColorImage(i), fbo->GetColorFormat(i),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
    if (fbo->HasDepth())
    {
        VulkanGraphicsContext::TransitionImageLayout(
            cmd, fbo->GetDepthImage(), fbo->GetDepthFormat(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }

    VkClearValue colorClear{};
    colorClear.color.float32[0] = m_ClearColor.r;
    colorClear.color.float32[1] = m_ClearColor.g;
    colorClear.color.float32[2] = m_ClearColor.b;
    colorClear.color.float32[3] = m_ClearColor.a;

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView    = fbo->GetColorImageView(0);
    colorAttachment.imageLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue   = colorClear;

    VkRenderingAttachmentInfo depthAttachment{};
    if (fbo->HasDepth())
    {
        VkClearValue depthClear{};
        depthClear.depthStencil.depth  = 1.0f;
        depthClear.depthStencil.stencil = 0;
        depthAttachment.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView    = fbo->GetDepthImageView();
        depthAttachment.imageLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue  = depthClear;
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType               = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset   = { 0, 0 };
    renderingInfo.renderArea.extent   = { fbo->GetWidth(), fbo->GetHeight() };
    renderingInfo.layerCount          = 1;
    renderingInfo.colorAttachmentCount = 1; // single color attachment this iteration
    renderingInfo.pColorAttachments   = &colorAttachment;
    renderingInfo.pDepthAttachment    = fbo->HasDepth() ? &depthAttachment : nullptr;
    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(fbo->GetWidth());
    viewport.height   = static_cast<float>(fbo->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { fbo->GetWidth(), fbo->GetHeight() };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_ActiveColorFormat = fbo->GetColorFormat(0);
    m_ActiveDepthFormat = fbo->GetDepthFormat();
    m_InOffscreenPass   = true;
}

void VulkanRendererAPI::EndRenderPass()
{
    if (!m_InOffscreenPass)
        return; // swapchain pass: stays open until SwapBuffers (EndFrame).

    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
        return;
    VkCommandBuffer cmd = ctx.GetCurrentCommandBuffer();
    auto* fbo = m_ActiveFrameBuffer;

    vkCmdEndRendering(cmd); // end the offscreen pass

    // Transition the FBO color/depth images -> SHADER_READ_ONLY so the rendered
    // result can be sampled by a later pass (post-process, shadow maps, ...).
    if (fbo)
    {
        for (uint32_t i = 0; i < fbo->GetColorAttachmentCount(); ++i)
        {
            VulkanGraphicsContext::TransitionImageLayout(
                cmd, fbo->GetColorImage(i), fbo->GetColorFormat(i),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
        if (fbo->HasDepth())
        {
            VulkanGraphicsContext::TransitionImageLayout(
                cmd, fbo->GetDepthImage(), fbo->GetDepthFormat(),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
    }

    // Restart the swapchain pass with loadOp = LOAD so earlier scene output is
    // preserved and subsequent scene layers + ImGui keep drawing to the swapchain.
    auto& swap = ctx.GetSwapchain();
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView    = swap.GetImageView(ctx.GetCurrentImageIndex());
    colorAttachment.imageLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp       = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView    = swap.GetDepthView();
    depthAttachment.imageLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp       = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType               = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset   = { 0, 0 };
    renderingInfo.renderArea.extent   = swap.GetExtent();
    renderingInfo.layerCount          = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments   = &colorAttachment;
    renderingInfo.pDepthAttachment    = &depthAttachment;
    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swap.GetExtent().width);
    viewport.height   = static_cast<float>(swap.GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swap.GetExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_ActiveColorFormat = swap.GetImageFormat();
    m_ActiveDepthFormat = swap.GetDepthFormat();
    m_InOffscreenPass   = false;
    m_ActiveFrameBuffer = nullptr;
}
void VulkanRendererAPI::SetViewport(int x, int y, int width, int height)
{
    m_ViewportX = x; m_ViewportY = y;
    m_ViewportW = width; m_ViewportH = height;

    auto& ctx = VulkanGraphicsContext::Get();
    if (ctx.IsFrameStarted() && width > 0 && height > 0)
    {
        VkViewport viewport{};
        viewport.x        = static_cast<float>(x);
        viewport.y        = static_cast<float>(y);
        viewport.width     = static_cast<float>(width);
        viewport.height    = static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(ctx.GetCurrentCommandBuffer(), 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { x, y };
        scissor.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        vkCmdSetScissor(ctx.GetCurrentCommandBuffer(), 0, 1, &scissor);
    }
}

// ── Draw ──────────────────────────────────────────────────────────

void VulkanRendererAPI::DrawIndexed(const VertexArray& vertexArray)
{
    DrawIndexedCommon(vertexArray, 1, 0);
}

void VulkanRendererAPI::DrawIndexedInstanced(const VertexArray& vertexArray,
                                             uint32_t instanceCount,
                                             uint32_t baseInstance)
{
    DrawIndexedCommon(vertexArray, instanceCount, baseInstance);
}

void VulkanRendererAPI::DrawIndexedCommon(const VertexArray& vertexArray,
                                          uint32_t instanceCount,
                                          uint32_t firstInstance)
{
    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
        return;

    DMGE_CORE_ASSERT(m_CurrentShader, "Vulkan: DrawIndexed called with no shader bound!");
    auto& shader = *m_CurrentShader;

    const auto& va = static_cast<const VulkanVertexArray&>(vertexArray);
    VkCommandBuffer cmd = ctx.GetCurrentCommandBuffer();
    uint32_t frame = ctx.GetCurrentFrame();

    VkDeviceSize dynamicOffset = 0;
    bool hasUBO = (shader.GetUniformBlockSize() > 0);
    if (hasUBO)
        dynamicOffset = CommitUniforms(shader, frame);

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (shader.GetDescriptorSetLayout() != VK_NULL_HANDLE)
    {
        // P0-1: reuse the set from a previous draw with identical bindings
        // (same shader + same texture handles) instead of allocating and
        // rewriting one per draw. The UBO is dynamic: only the offset
        // passed to vkCmdBindDescriptorSets changes per draw.
        DescKey key{ shader.GetID(), HashBoundTextures(shader) };
        auto& cache = DescriptorCache(frame);
        auto it = cache.find(key);
        if (it != cache.end())
        {
            set = it->second;
        }
        else
        {
            set = AllocateDescriptorSet(shader.GetDescriptorSetLayout(), frame);
            if (set != VK_NULL_HANDLE)
            {
                WriteDescriptorSet(set, shader, frame);
                cache.emplace(key, set);
            }
        }
    }

    va.Bind();

    VkPipeline pipeline = GetOrCreatePipeline(shader, va);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    if (set != VK_NULL_HANDLE)
    {
        uint32_t dynCount = hasUBO ? 1 : 0;
        uint32_t dynOffset = hasUBO ? static_cast<uint32_t>(dynamicOffset) : 0;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                shader.GetPipelineLayout(),
                                0, 1, &set, dynCount,
                                hasUBO ? &dynOffset : nullptr);
    }

    vkCmdDrawIndexed(cmd, va.GetIndexCount(), instanceCount, 0, 0, firstInstance);
}

// ── Pipeline state setters ────────────────────────────────────────

void VulkanRendererAPI::SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor)
{
    m_BlendEnabled = enable;
    m_SrcBlend = srcFactor;
    m_DstBlend = dstFactor;
}

void VulkanRendererAPI::SetBlendEquation(BlendEquation equation)
{
    m_BlendEquation = equation;
}

void VulkanRendererAPI::SetDepthTest(bool enable) { m_DepthTestEnabled = enable; }
void VulkanRendererAPI::SetDepthFunc(DepthFunc func) { m_DepthFunc = func; }
void VulkanRendererAPI::SetCullMode(CullMode mode) { m_CullMode = mode; }

void VulkanRendererAPI::SetBoundTexture(uint32_t slot, VulkanTexture* texture)
{
    if (slot < kMaxBoundTextures)
        m_BoundTextures[slot] = texture;
}

// ── Pipeline key ──────────────────────────────────────────────────

bool VulkanRendererAPI::PipelineKey::operator==(const PipelineKey& o) const
{
    return shaderID         == o.shaderID &&
           vertexLayoutHash == o.vertexLayoutHash &&
           blendEnabled     == o.blendEnabled &&
           srcBlend          == o.srcBlend &&
           dstBlend          == o.dstBlend &&
           blendEq           == o.blendEq &&
           depthTestEnabled == o.depthTestEnabled &&
           depthFunc         == o.depthFunc &&
           cullMode          == o.cullMode &&
           colorFormat      == o.colorFormat &&
           depthFormat      == o.depthFormat;
}

size_t VulkanRendererAPI::PipelineKeyHash::operator()(const PipelineKey& k) const noexcept
{
    size_t h = std::hash<uint64_t>{}(k.shaderID);
    h ^= std::hash<uint64_t>{}(k.vertexLayoutHash) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(k.blendEnabled) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.srcBlend)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.dstBlend)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.blendEq)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(k.depthTestEnabled) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.depthFunc)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.cullMode)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(k.colorFormat)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(k.depthFormat)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

VkPipeline VulkanRendererAPI::GetOrCreatePipeline(VulkanShader& shader,
                                                    const VulkanVertexArray& va)
{
    PipelineKey key;
    key.shaderID          = shader.GetID();
    key.vertexLayoutHash  = va.GetLayoutHash();
    key.blendEnabled       = m_BlendEnabled;
    key.srcBlend           = m_SrcBlend;
    key.dstBlend           = m_DstBlend;
    key.blendEq            = m_BlendEquation;
    key.depthTestEnabled   = m_DepthTestEnabled;
    key.depthFunc          = m_DepthFunc;
    key.cullMode           = m_CullMode;
    key.colorFormat        = m_ActiveColorFormat;
    key.depthFormat        = m_ActiveDepthFormat;

    auto it = m_Pipelines.find(key);
    if (it != m_Pipelines.end())
        return it->second;

    auto& dev   = VulkanDevice::Get();
    VkFormat colorFormat = m_ActiveColorFormat;
    VkFormat depthFormat  = m_ActiveDepthFormat;

    // ── Shader stages ────────────────────────────────────────────
    const auto& stages = shader.GetShaderStages();

    // ── Vertex input ─────────────────────────────────────────────
    const auto& bindings   = va.GetBindingDescriptions();
    const auto& attributes  = va.GetAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount    = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions       = bindings.data();
    vertexInput.vertexAttributeDescriptionCount  = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions     = attributes.data();

    // ── Input assembly ───────────────────────────────────────────
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ── Viewport / scissor (dynamic) ─────────────────────────────
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount   = 1;

    // ── Rasterizer ───────────────────────────────────────────────
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType        = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = CullModeToVk(m_CullMode);
    rasterizer.frontFace    = VK_FRONT_FACE_CLOCKWISE;  // CW: matches the projection Y-flip in Renderer::BeginScene
    rasterizer.lineWidth   = 1.0f;

    // ── Multisampling ────────────────────────────────────────────
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable   = VK_FALSE;

    // ── Depth / stencil ──────────────────────────────────────────
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable   = m_DepthTestEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable  = m_DepthTestEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp    = DepthFuncToVk(m_DepthFunc);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable    = VK_FALSE;

    // ── Color blend ───────────────────────────────────────────────
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable         = m_BlendEnabled ? VK_TRUE : VK_FALSE;
    blendAttachment.srcColorBlendFactor = BlendFactorToVk(m_SrcBlend);
    blendAttachment.dstColorBlendFactor = BlendFactorToVk(m_DstBlend);
    blendAttachment.colorBlendOp         = BlendEquationToVk(m_BlendEquation);
    blendAttachment.srcAlphaBlendFactor = BlendFactorToVk(m_SrcBlend);
    blendAttachment.dstAlphaBlendFactor = BlendFactorToVk(m_DstBlend);
    blendAttachment.alphaBlendOp         = BlendEquationToVk(m_BlendEquation);
    blendAttachment.colorWriteMask       = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable   = VK_FALSE;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments     = &blendAttachment;

    // ── Dynamic state ────────────────────────────────────────────
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount  = 2;
    dynamicState.pDynamicStates      = dynamicStates;

    // ── Dynamic rendering info ───────────────────────────────────
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount     = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat    = depthFormat;

    // ── Assemble ─────────────────────────────────────────────────
    VkGraphicsPipelineCreateInfo info{};
    info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext               = &renderingInfo;
    info.stageCount           = static_cast<uint32_t>(stages.size());
    info.pStages              = stages.data();
    info.pVertexInputState    = &vertexInput;
    info.pInputAssemblyState   = &inputAssembly;
    info.pViewportState        = &viewportState;
    info.pRasterizationState   = &rasterizer;
    info.pMultisampleState     = &multisampling;
    info.pDepthStencilState   = &depthStencil;
    info.pColorBlendState      = &colorBlend;
    info.pDynamicState         = &dynamicState;
    info.layout                = shader.GetPipelineLayout();
    info.renderPass            = VK_NULL_HANDLE; // dynamic rendering
    info.subpass               = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(dev.Device, m_PipelineCache, 1, &info, nullptr, &pipeline));

    m_Pipelines[key] = pipeline;
    return pipeline;
}

// ── Per-frame scratch resources ───────────────────────────────────

void VulkanRendererAPI::CreateFrameResources()
{
    auto& dev = VulkanDevice::Get();

    VkDeviceSize align = dev.MinUniformBufferOffsetAlignment;
    if (align == 0) align = 16;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        // Descriptor pool: one set per draw (UBO + samplers).
        VkDescriptorPoolSize poolSizes[2] = {};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        poolSizes[0].descriptorCount = 8192;
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 32768;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 8192;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes    = poolSizes;
        VK_CHECK(vkCreateDescriptorPool(dev.Device, &poolInfo, nullptr, &m_DescriptorPools[i]));

        // Host-visible uniform scratch buffer.
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size  = kUniformBufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocResult{};
        VK_CHECK(vmaCreateBuffer(dev.Allocator, &bufInfo, &allocInfo,
                                 &m_UniformBuffers[i], &m_UniformAllocs[i], &allocResult));
        m_UniformMapped[i] = allocResult.pMappedData;
        m_UniformOffset[i] = 0;
    }
}

void VulkanRendererAPI::DestroyFrameResources()
{
    auto& dev = VulkanDevice::Get();
    if (dev.Device == VK_NULL_HANDLE)
        return;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (m_UniformBuffers[i] != VK_NULL_HANDLE)
            vmaDestroyBuffer(dev.Allocator, m_UniformBuffers[i], m_UniformAllocs[i]);
        if (m_DescriptorPools[i] != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(dev.Device, m_DescriptorPools[i], nullptr);
        m_UniformBuffers[i] = VK_NULL_HANDLE;
        m_DescriptorPools[i] = VK_NULL_HANDLE;
        m_UniformMapped[i] = nullptr;
        m_UniformOffset[i] = 0;
    }
}

void VulkanRendererAPI::CreateDummyResources()
{
    auto& dev = VulkanDevice::Get();

    // 1x1 RGBA8 image for unbound sampler slots. No pixel data is uploaded:
    // an UNDEFINED -> SHADER_READ_ONLY_OPTIMAL transition is valid and leaves
    // contents undefined (the dummy only exists to satisfy the descriptor
    // binding; sampling it returns 0/black, acceptable for an unused slot).
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = { 1, 1, 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    VK_CHECK(vmaCreateImage(dev.Allocator, &imageInfo, &allocInfo,
                            &m_DummyImage, &m_DummyAlloc, nullptr));

    dev.ImmediateSubmit([this](VkCommandBuffer cmd) {
        VulkanGraphicsContext::TransitionImageLayout(cmd, m_DummyImage,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    });

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image            = m_DummyImage;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    VK_CHECK(vkCreateImageView(dev.Device, &viewInfo, nullptr, &m_DummyView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter     = VK_FILTER_NEAREST;
    samplerInfo.minFilter     = VK_FILTER_NEAREST;
    samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy    = 1.0f;
    samplerInfo.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable   = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.minLod           = 0.0f;
    samplerInfo.maxLod           = 0.0f;
    VK_CHECK(vkCreateSampler(dev.Device, &samplerInfo, nullptr, &m_DummySampler));
}

void VulkanRendererAPI::DestroyDummyResources()
{
    auto& dev = VulkanDevice::Get();
    if (dev.Device == VK_NULL_HANDLE)
        return;

    if (m_DummySampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(dev.Device, m_DummySampler, nullptr);
        m_DummySampler = VK_NULL_HANDLE;
    }
    if (m_DummyView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(dev.Device, m_DummyView, nullptr);
        m_DummyView = VK_NULL_HANDLE;
    }
    if (m_DummyImage != VK_NULL_HANDLE)
    {
        vmaDestroyImage(dev.Allocator, m_DummyImage, m_DummyAlloc);
        m_DummyImage = VK_NULL_HANDLE;
        m_DummyAlloc = VK_NULL_HANDLE;
    }
}

void VulkanRendererAPI::ResetFrame(uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    vkResetDescriptorPool(dev.Device, m_DescriptorPools[frameIndex], 0);
    // The pool reset invalidates every descriptor set allocated from it, so
    // the frame's cache must drop all entries (P0-1).
    DescriptorCache(frameIndex).clear();
    m_UniformOffset[frameIndex] = 0;
}

void VulkanRendererAPI::OnShaderDestroyed(uint64_t shaderID)
{
    // Defensive invalidation (see declaration comment): shader IDs are never
    // reused and the per-frame ResetFrame clear already bounds entry
    // lifetime, but purge eagerly so the maps never hold dead-shader keys.
    // The sets themselves are pool-owned and die at the pool reset.
    PurgeShaderFromDescriptorCache(shaderID);
}

uint64_t VulkanRendererAPI::HashBoundTextures(const VulkanShader& shader) const
{
    uint64_t h = 1469598103934665603ull; // FNV-1a offset basis
    for (uint32_t i = 0; i < shader.GetSamplerCount(); ++i)
    {
        VulkanTexture* tex = (i < kMaxBoundTextures) ? m_BoundTextures[i] : nullptr;
        // Unbound slots write the global dummy - hash the exact handles the
        // descriptor will contain so equivalent bindings collide on purpose.
        // (uintptr_t via reinterpret_cast: non-dispatchable handles are
        // pointer-typed with this SDK's headers, integer-typed otherwise.)
        VkSampler   sampler = tex ? tex->GetVkSampler()   : m_DummySampler;
        VkImageView view    = tex ? tex->GetVkImageView() : m_DummyView;
        h ^= static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sampler)) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= static_cast<uint64_t>(reinterpret_cast<uintptr_t>(view))    + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
}

uint32_t VulkanRendererAPI::CurrentFrameIndex() const
{
    return VulkanGraphicsContext::Get().GetCurrentFrame();
}

VkDeviceSize VulkanRendererAPI::CommitUniforms(VulkanShader& shader, uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    VkDeviceSize blockSize = shader.GetUniformBlockSize();
    VkDeviceSize align     = dev.MinUniformBufferOffsetAlignment;
    if (align == 0) align = 16;

    VkDeviceSize alignedSize = AlignUp(blockSize, align);
    VkDeviceSize offset = AlignUp(m_UniformOffset[frameIndex], align);

    if (offset + alignedSize > kUniformBufferSize)
    {
        DMGE_CORE_ASSERT(false, "Vulkan: per-frame uniform scratch buffer exhausted!");
        return 0;
    }

    std::memcpy(static_cast<uint8_t*>(m_UniformMapped[frameIndex]) + offset,
                shader.GetUniformData(), static_cast<size_t>(blockSize));

    m_UniformOffset[frameIndex] = offset + alignedSize;
    return offset;
}

VkDescriptorSet VulkanRendererAPI::AllocateDescriptorSet(VkDescriptorSetLayout layout, uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = m_DescriptorPools[frameIndex];
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(dev.Device, &info, &set);
    if (result != VK_SUCCESS)
    {
        DMGE_LOG_ERROR("Vulkan: failed to allocate descriptor set ({0})", static_cast<int>(result));
        return VK_NULL_HANDLE;
    }
    return set;
}

void VulkanRendererAPI::WriteDescriptorSet(VkDescriptorSet set, VulkanShader& shader,
                                            uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    VkDeviceSize blockSize = shader.GetUniformBlockSize();

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(1 + shader.GetSamplerCount());

    VkDescriptorBufferInfo uboInfo{};
    VkWriteDescriptorSet uboWrite{};
    if (blockSize > 0)
    {
        uboInfo.buffer = m_UniformBuffers[frameIndex];
        uboInfo.offset = 0;
        uboInfo.range  = blockSize;

        uboWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet           = set;
        uboWrite.dstBinding       = 0;
        uboWrite.dstArrayElement  = 0;
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        uboWrite.pBufferInfo      = &uboInfo;
        writes.push_back(uboWrite);
    }

    // Samplers: binding 1..N filled from the currently bound texture slots.
    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(shader.GetSamplerCount());
    for (uint32_t i = 0; i < shader.GetSamplerCount(); ++i)
    {
        VulkanTexture* tex = (i < kMaxBoundTextures) ? m_BoundTextures[i] : nullptr;
        VkDescriptorImageInfo info{};
        if (tex)
        {
            info.sampler   = tex->GetVkSampler();
            info.imageView = tex->GetVkImageView();
        }
        else
        {
            // Unbound sampler slot: bind the global 1x1 dummy so the
            // descriptor is valid (sampling returns black) instead of
            // leaving the binding unwritten, which triggers
            // "descriptor set encountered uninitialized binding"
            // validation and undefined sampling results.
            info.sampler   = m_DummySampler;
            info.imageView = m_DummyView;
        }
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos.push_back(info);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet           = set;
        write.dstBinding       = i + 1; // samplers start at binding 1
        write.dstArrayElement  = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo       = &imageInfos.back();
        writes.push_back(write);
    }

    if (!writes.empty())
        vkUpdateDescriptorSets(dev.Device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
}

} // namespace DMGameEngine
