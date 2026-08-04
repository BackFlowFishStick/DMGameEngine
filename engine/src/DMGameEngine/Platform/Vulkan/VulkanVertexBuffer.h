/*
 * DMGameEngine - Vulkan Vertex Buffer
 *
 * Vulkan implementation of the VertexBuffer abstraction.
 *  - Constructed with data   -> GPU-local buffer (static draw).
 *  - Constructed with size   -> host-visible buffer (dynamic draw),
 *    updated via SetData() through a persistently mapped pointer.
 */

#pragma once

#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstdint>

namespace DMGameEngine {

class DMGE_API VulkanVertexBuffer : public VertexBuffer
{
public:
    explicit VulkanVertexBuffer(uint32_t size);
    VulkanVertexBuffer(const void* vertices, uint32_t size);
    ~VulkanVertexBuffer() override;

    void Bind()   const override {}
    void Unbind() const override {}

    void  SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
    const BufferLayout& GetLayout() const override { return m_Layout; }

    void SetData(const void* data, uint32_t size) override;

    VkBuffer GetVkBuffer() const { return m_Buffer; }

private:
    VkBuffer       m_Buffer  = VK_NULL_HANDLE;
    VmaAllocation  m_Alloc   = VK_NULL_HANDLE;
    void*          m_Mapped  = nullptr;   // non-null for dynamic buffers
    uint32_t       m_Size    = 0;
    BufferLayout   m_Layout;
};

} // namespace DMGameEngine
