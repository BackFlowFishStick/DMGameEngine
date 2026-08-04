/*
 * DMGameEngine - Vulkan Index Buffer
 *
 * Vulkan implementation of the IndexBuffer abstraction. Index data is
 * uploaded once (GPU-local) via a staging buffer; indices are uint32.
 */

#pragma once

#include "DMGameEngine/Renderer/IndexBuffer.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstdint>

namespace DMGameEngine {

class DMGE_API VulkanIndexBuffer : public IndexBuffer
{
public:
    VulkanIndexBuffer(const uint32_t* indices, uint32_t count);
    ~VulkanIndexBuffer() override;

    void Bind()   const override {}
    void Unbind() const override {}

    uint32_t GetCount() const override { return m_Count; }

    VkBuffer GetVkBuffer() const { return m_Buffer; }

private:
    VkBuffer      m_Buffer = VK_NULL_HANDLE;
    VmaAllocation m_Alloc   = VK_NULL_HANDLE;
    uint32_t      m_Count   = 0;
};

} // namespace DMGameEngine
