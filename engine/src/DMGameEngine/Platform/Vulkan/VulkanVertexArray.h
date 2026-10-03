/*
 * DMGameEngine - Vulkan Vertex Array
 *
 * Vulkan has no VAO object; this class records the vertex-input
 * configuration (binding + attribute descriptions derived from the
 * attached VertexBuffers' layouts) and binds the underlying VkBuffers
 * + IndexBuffer to the active command buffer at draw time.
 */

#pragma once

#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Platform/Vulkan/VulkanVertexBuffer.h"
#include "DMGameEngine/Platform/Vulkan/VulkanIndexBuffer.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace DMGameEngine {

class DMGE_API VulkanVertexArray : public VertexArray
{
public:
    VulkanVertexArray();
    ~VulkanVertexArray() override;

    void Bind()   const override;
    void Unbind() const override {}

    void AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer) override;
    void SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer)   override;

    const std::vector<DM::Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
    const DM::Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }

    // ── Vulkan vertex-input configuration (for pipeline creation) ──
    const std::vector<VkVertexInputBindingDescription>&   GetBindingDescriptions()   const { return m_BindingDescs; }
    const std::vector<VkVertexInputAttributeDescription>& GetAttributeDescriptions()  const { return m_AttributeDescs; }
    uint64_t GetLayoutHash() const { return m_LayoutHash; }

    uint32_t GetIndexCount() const;

private:
    // Maximum vertex buffers bindable in one vkCmdBindVertexBuffers call.
    // Real meshes use 1-2 (vertex + instance) streams; anything beyond the
    // capacity is rejected with an error log at AddVertexBuffer time (never
    // per draw).
    static constexpr uint32_t kMaxVertexBindings = 8;

    void RebuildLayout();

    std::vector<DM::Ref<VertexBuffer>> m_VertexBuffers;
    DM::Ref<IndexBuffer>                m_IndexBuffer;

    std::vector<VkVertexInputBindingDescription>   m_BindingDescs;
    std::vector<VkVertexInputAttributeDescription> m_AttributeDescs;
    uint64_t m_LayoutHash = 0;

    // ── Bind() fast path (P1: no per-draw heap allocation / RTTI) ──
    // The VkBuffer handles are resolved ONCE, in AddVertexBuffer /
    // SetIndexBuffer, and cached here. VulkanVertexBuffer/VulkanIndexBuffer
    // never reallocate their VkBuffer (SetData only updates contents and the
    // Refs in m_VertexBuffers keep the objects alive), so the handles stay
    // valid for the array's lifetime. Fixed C arrays, not STL: R1 forbids
    // new STL members on an exported class.
    VkBuffer m_VkBuffers[kMaxVertexBindings] = {};
    uint32_t m_BufferCount                   = 0;
    VkBuffer m_VkIndexBuffer                 = VK_NULL_HANDLE;
};

} // namespace DMGameEngine
