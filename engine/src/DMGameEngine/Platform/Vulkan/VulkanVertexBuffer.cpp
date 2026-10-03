/*
 * DMGameEngine - Vulkan Vertex Buffer Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanVertexBuffer.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"

#include "DMGameEngine/Core/Log.h"

#include <cstring>

namespace DMGameEngine {

namespace {

void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                  VmaMemoryUsage memUsage,
                  VkBuffer& outBuffer, VmaAllocation& outAlloc, void** outMapped)
{
    auto& dev = VulkanDevice::Get();

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memUsage;
    if (outMapped)
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocResult{};
    VK_CHECK(vmaCreateBuffer(dev.Allocator, &bufInfo, &allocInfo,
                             &outBuffer, &outAlloc, &allocResult));
    if (outMapped)
        *outMapped = allocResult.pMappedData;
}

} // anonymous namespace

// ── Constructors / Destructor ─────────────────────────────────────

VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size)
    : m_Size(size)
{
    // Dynamic-draw buffer: host-visible + persistently mapped.
    CreateBuffer(size,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                 m_Buffer, m_Alloc, &m_Mapped);
}

VulkanVertexBuffer::VulkanVertexBuffer(const void* vertices, uint32_t size)
    : m_Size(size)
{
    // Static-draw buffer: GPU-local, uploaded through a staging buffer.
    VkBuffer       staging = VK_NULL_HANDLE;
    VmaAllocation  stagingAlloc = VK_NULL_HANDLE;
    void*          stagingMapped = nullptr;
    CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_MEMORY_USAGE_CPU_ONLY,
                 staging, stagingAlloc, &stagingMapped);
    std::memcpy(stagingMapped, vertices, size);

    CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VMA_MEMORY_USAGE_GPU_ONLY,
                 m_Buffer, m_Alloc, nullptr);

    VulkanDevice::Get().ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy region{};
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size      = size;
        vkCmdCopyBuffer(cmd, staging, m_Buffer, 1, &region);
    });

    vmaDestroyBuffer(VulkanDevice::Get().Allocator, staging, stagingAlloc);
}

VulkanVertexBuffer::~VulkanVertexBuffer()
{
    // Deferred (review item B): an in-flight frame may still bind this
    // buffer; destroying here would be use-while-in-flight.
    VulkanDevice::DeferDestroyBuffer(m_Buffer, m_Alloc);
    m_Buffer = VK_NULL_HANDLE;
    m_Alloc = nullptr;
}

// ── Data upload ───────────────────────────────────────────────────

void VulkanVertexBuffer::SetData(const void* data, uint32_t size)
{
    if (m_Mapped)
    {
        // Dynamic buffer: write directly into mapped memory.
        uint32_t toWrite = (size < m_Size) ? size : m_Size;
        std::memcpy(m_Mapped, data, toWrite);
    }
    else
    {
        // Static buffer: re-upload through a staging copy.
        VkBuffer       staging = VK_NULL_HANDLE;
        VmaAllocation  stagingAlloc = VK_NULL_HANDLE;
        void*          stagingMapped = nullptr;
        CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VMA_MEMORY_USAGE_CPU_ONLY,
                     staging, stagingAlloc, &stagingMapped);
        std::memcpy(stagingMapped, data, size);

        VulkanDevice::Get().ImmediateSubmit([&](VkCommandBuffer cmd) {
            VkBufferCopy region{};
            region.size = size;
            vkCmdCopyBuffer(cmd, staging, m_Buffer, 1, &region);
        });

        vmaDestroyBuffer(VulkanDevice::Get().Allocator, staging, stagingAlloc);
    }
}

} // namespace DMGameEngine
