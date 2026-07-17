/*
 * DMGameEngine - Vulkan Vertex Array Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanVertexArray.h"
#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTextureUtils.h"

#include "DMGameEngine/Core/Log.h"

#include <functional>

namespace DMGameEngine {

namespace {

void HashCombine(uint64_t& h, uint64_t v)
{
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
}

} // anonymous namespace

VulkanVertexArray::VulkanVertexArray() = default;

VulkanVertexArray::~VulkanVertexArray() = default;

void VulkanVertexArray::RebuildLayout()
{
    m_BindingDescs.clear();
    m_AttributeDescs.clear();
    m_LayoutHash = 0;

    uint32_t bindingIndex = 0;
    uint32_t location = 0;

    for (const auto& vb : m_VertexBuffers)
    {
        const auto& layout = vb->GetLayout();
        DMGE_CORE_ASSERT(!layout.GetElements().empty(),
                         "Vertex buffer has no layout!");

        VkVertexInputBindingDescription binding{};
        binding.binding   = bindingIndex;
        binding.stride    = layout.GetStride();
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        m_BindingDescs.push_back(binding);

        HashCombine(m_LayoutHash, bindingIndex);
        HashCombine(m_LayoutHash, layout.GetStride());

        for (const auto& elem : layout.GetElements())
        {
            // Matrix types occupy multiple consecutive locations (one per column).
            uint32_t cols = 1;
            if (elem.Type == ShaderDataType::Mat4) cols = 4;
            else if (elem.Type == ShaderDataType::Mat3) cols = 3;

            for (uint32_t c = 0; c < cols; ++c)
            {
                VkVertexInputAttributeDescription attr{};
                attr.location = location;
                attr.binding   = bindingIndex;
                attr.format    = Detail::ShaderDataTypeToVkFormat(elem.Type);
                attr.offset    = elem.Offset + c * 16;
                m_AttributeDescs.push_back(attr);

                HashCombine(m_LayoutHash, location);
                HashCombine(m_LayoutHash, bindingIndex);
                HashCombine(m_LayoutHash, static_cast<uint64_t>(attr.format));
                HashCombine(m_LayoutHash, attr.offset);

                ++location;
            }
        }

        ++bindingIndex;
    }
}

void VulkanVertexArray::AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer)
{
    DMGE_CORE_ASSERT(vertexBuffer, "VulkanVertexArray: vertex buffer is null!");
    m_VertexBuffers.push_back(vertexBuffer);
    RebuildLayout();
}

void VulkanVertexArray::SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer)
{
    DMGE_CORE_ASSERT(indexBuffer, "VulkanVertexArray: index buffer is null!");
    m_IndexBuffer = indexBuffer;
}

uint32_t VulkanVertexArray::GetIndexCount() const
{
    return m_IndexBuffer ? m_IndexBuffer->GetCount() : 0;
}

void VulkanVertexArray::Bind() const
{
    if (m_VertexBuffers.empty())
        return;

    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
        return;

    VkCommandBuffer cmd = ctx.GetCurrentCommandBuffer();

    // Bind all vertex buffers in one call (one binding each).
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceSize> offsets;
    buffers.reserve(m_VertexBuffers.size());
    offsets.reserve(m_VertexBuffers.size());
    for (uint32_t i = 0; i < m_VertexBuffers.size(); ++i)
    {
        const auto& vb = std::dynamic_pointer_cast<VulkanVertexBuffer>(m_VertexBuffers[i]);
        DMGE_CORE_ASSERT(vb, "VulkanVertexArray: expected a VulkanVertexBuffer!");
        buffers.push_back(vb->GetVkBuffer());
        offsets.push_back(0);
    }
    vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(buffers.size()),
                            buffers.data(), offsets.data());

    if (m_IndexBuffer)
    {
        const auto& ib = std::dynamic_pointer_cast<VulkanIndexBuffer>(m_IndexBuffer);
        DMGE_CORE_ASSERT(ib, "VulkanVertexArray: expected a VulkanIndexBuffer!");
        vkCmdBindIndexBuffer(cmd, ib->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

} // namespace DMGameEngine
