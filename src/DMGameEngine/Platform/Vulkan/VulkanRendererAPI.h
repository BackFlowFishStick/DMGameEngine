/*
 * DMGameEngine - Vulkan Renderer API
 *
 * Vulkan implementation of the RendererAPI abstraction. Because Vulkan
 * bakes shader + vertex-layout + blend/depth/cull state into a single
 * VkPipeline (unlike OpenGL's per-call glEnable state), this backend
 * keeps a small dynamic-state cache:
 *
 *   - a VkPipeline cache keyed by (shader, vertex layout, blend, depth,
 *     cull mode); pipelines are created lazily on the first draw that
 *     needs them and reused thereafter.
 *   - a per-frame scratch uniform buffer (host-visible) + per-frame
 *     descriptor pool. VulkanShader commits its named uniforms into a
 *     fresh slot of the scratch buffer per draw and binds it through a
 *     dynamic UBO descriptor, so the OpenGL per-draw Set*()/Material
 *     semantics are reproduced exactly.
 *
 * Frame lifecycle (driven by the context):
 *   Clear()      -> context.BeginFrame() + reset per-frame scratch.
 *   DrawIndexed  -> commit uniforms, allocate a descriptor set, bind the
 *                   (cached) pipeline + vertex/index buffers, draw.
 *   (SwapBuffers)-> context.EndFrame() (submit + present).
 */

#pragma once

#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <unordered_map>
#include <cstdint>

namespace DMGameEngine {

class VulkanShader;
class VulkanTexture;
class VulkanVertexArray;
class VulkanFrameBuffer;

class DMGE_API VulkanRendererAPI : public RendererAPI
{
public:
    VulkanRendererAPI() = default;
    ~VulkanRendererAPI() override;

    void Init(const RendererAPIInitConfig& config = {}) override;

    void SetClearColor(const glm::vec4& color) override { m_ClearColor = color; }
    void Clear() override;
    void SetViewport(int x, int y, int width, int height) override;
    void DrawIndexed(const VertexArray& vertexArray) override;
    void DrawIndexedInstanced(const VertexArray& vertexArray, uint32_t instanceCount, uint32_t baseInstance = 0) override;

    void SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor) override;
    void SetBlendEquation(BlendEquation equation) override;
    void SetDepthTest(bool enable) override;
    void SetDepthFunc(DepthFunc func) override;
    void SetCullMode(CullMode mode) override;

    // -- Render pass / target ----------------------------------------
    // See RendererAPI. Begins/ends a dynamic-rendering pass targeting target
    // (nullptr = swapchain, already open from BeginFrame). An offscreen
    // FrameBuffer temporarily replaces the swapchain pass (multi-pass RTT).
    void BeginRenderPass(FrameBuffer* target) override;
    void EndRenderPass() override;

    // ── Backend interop hooks ───────────────────────────────────
    // VulkanShader/VulkanTexture call these during Bind() so DrawIndexed
    // knows the active shader / bound texture slots.
    void SetCurrentShader(VulkanShader* shader) { m_CurrentShader = shader; }
    void SetBoundTexture(uint32_t slot, VulkanTexture* texture);

    static VulkanRendererAPI* Get() { return s_Instance; }

private:
    static constexpr uint32_t kMaxFramesInFlight = 2;
    static constexpr uint32_t kMaxBoundTextures  = 32;
    static constexpr VkDeviceSize kUniformBufferSize = 16ull * 1024 * 1024; // 16 MB / frame

    // ── Pipeline cache ──────────────────────────────────────────
    struct PipelineKey
    {
        uint64_t shaderID        = 0;
        uint64_t vertexLayoutHash = 0;
        bool     blendEnabled    = false;
        BlendFactor srcBlend      = BlendFactor::SrcAlpha;
        BlendFactor dstBlend      = BlendFactor::OneMinusSrcAlpha;
        BlendEquation blendEq     = BlendEquation::Add;
        bool     depthTestEnabled = true;
        DepthFunc depthFunc       = DepthFunc::Less;
        CullMode cullMode         = CullMode::None;
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;

        bool operator==(const PipelineKey& other) const;
    };
    struct PipelineKeyHash
    {
        size_t operator()(const PipelineKey& k) const noexcept;
    };

    VkPipeline GetOrCreatePipeline(VulkanShader& shader, const VulkanVertexArray& vertexArray);
    // Shared by DrawIndexed (1 instance) and DrawIndexedInstanced (N).
    void DrawIndexedCommon(const VertexArray& vertexArray,
                           uint32_t instanceCount,
                           uint32_t firstInstance);

    // ── Per-frame scratch ───────────────────────────────────────
    void CreateFrameResources();
    void DestroyFrameResources();
    void ResetFrame(uint32_t frameIndex);
    uint32_t CurrentFrameIndex() const;
    VkDeviceSize CommitUniforms(VulkanShader& shader, uint32_t frameIndex);
    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout, uint32_t frameIndex);
    void WriteDescriptorSet(VkDescriptorSet set, VulkanShader& shader,
                             uint32_t frameIndex, VkDeviceSize dynamicOffset);

    // ── State ──────────────────────────────────────────────────
    glm::vec4   m_ClearColor       { 0.0f, 0.0f, 0.0f, 0.0f };
    bool        m_BlendEnabled      = false;
    BlendFactor m_SrcBlend          = BlendFactor::SrcAlpha;
    BlendFactor m_DstBlend          = BlendFactor::OneMinusSrcAlpha;
    BlendEquation m_BlendEquation    = BlendEquation::Add;
    bool        m_DepthTestEnabled   = true;
    DepthFunc   m_DepthFunc          = DepthFunc::Less;
    CullMode    m_CullMode           = CullMode::None;

    // Cached viewport (recorded when a frame is active).
    int m_ViewportX = 0, m_ViewportY = 0, m_ViewportW = 0, m_ViewportH = 0;
    // Active render-target formats (set when a pass begins). Fed into the
    // PipelineKey so an offscreen HDR target (e.g. RGBA16F) gets its own
    // cached pipeline instead of mismatching the swapchain format.
    VkFormat m_ActiveColorFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;
    bool            m_InOffscreenPass   = false;
    VulkanFrameBuffer* m_ActiveFrameBuffer = nullptr;

    VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> m_Pipelines;

    // Per frame-in-flight.
    VkDescriptorPool m_DescriptorPools[kMaxFramesInFlight] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkBuffer         m_UniformBuffers[kMaxFramesInFlight]   = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VmaAllocation    m_UniformAllocs[kMaxFramesInFlight]    = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    void*            m_UniformMapped[kMaxFramesInFlight]    = { nullptr, nullptr };
    VkDeviceSize     m_UniformOffset[kMaxFramesInFlight]    = { 0, 0 };

    VulkanShader*   m_CurrentShader  = nullptr;
    VulkanTexture*  m_BoundTextures[kMaxBoundTextures] = {};

    static VulkanRendererAPI* s_Instance;
};

} // namespace DMGameEngine
