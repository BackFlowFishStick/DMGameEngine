/*
 * DMGameEngine - Vulkan Renderer API Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"
#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanShader.h"
#include "DMGameEngine/Platform/Vulkan/VulkanVertexArray.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTexture.h"

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

    DMGE_LOG_INFO("Vulkan renderer API initialized.");
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
    // If a frame is already started, the framebuffer was cleared at
    // BeginFrame (loadOp = CLEAR); nothing more to do.
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
        set = AllocateDescriptorSet(shader.GetDescriptorSetLayout(), frame);
        if (set != VK_NULL_HANDLE)
            WriteDescriptorSet(set, shader, frame, dynamicOffset);
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
           cullMode          == o.cullMode;
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

    auto it = m_Pipelines.find(key);
    if (it != m_Pipelines.end())
        return it->second;

    auto& dev   = VulkanDevice::Get();
    auto& swap  = VulkanGraphicsContext::Get().GetSwapchain();
    VkFormat colorFormat = swap.GetImageFormat();
    VkFormat depthFormat  = swap.GetDepthFormat();

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

void VulkanRendererAPI::ResetFrame(uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    vkResetDescriptorPool(dev.Device, m_DescriptorPools[frameIndex], 0);
    m_UniformOffset[frameIndex] = 0;
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
                                            uint32_t frameIndex, VkDeviceSize dynamicOffset)
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
        if (!tex)
            continue; // unbound sampler slot: leave the binding unwritten

        info.sampler     = tex->GetVkSampler();
        info.imageView   = tex->GetVkImageView();
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
