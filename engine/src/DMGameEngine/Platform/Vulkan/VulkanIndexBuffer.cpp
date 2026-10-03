/*
 * DMGameEngine - Vulkan Index Buffer Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanIndexBuffer.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"

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

VulkanIndexBuffer::VulkanIndexBuffer(const uint32_t* indices, uint32_t count)
    : m_Count(count)
{
    VkDeviceSize size = static_cast<VkDeviceSize>(count) * sizeof(uint32_t);

    VkBuffer       staging = VK_NULL_HANDLE;
    VmaAllocation  stagingAlloc = VK_NULL_HANDLE;
    void*          stagingMapped = nullptr;
    CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_MEMORY_USAGE_CPU_ONLY,
                 staging, stagingAlloc, &stagingMapped);
    std::memcpy(stagingMapped, indices, size);

    CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VMA_MEMORY_USAGE_GPU_ONLY,
                 m_Buffer, m_Alloc, nullptr);

    VulkanDevice::Get().ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, staging, m_Buffer, 1, &region);
    });

    vmaDestroyBuffer(VulkanDevice::Get().Allocator, staging, stagingAlloc);
}

VulkanIndexBuffer::~VulkanIndexBuffer()
{
    // Deferred (review item B): an in-flight frame may still bind this
    // buffer; destroying here would be use-while-in-flight.
    VulkanDevice::DeferDestroyBuffer(m_Buffer, m_Alloc);
    m_Buffer = VK_NULL_HANDLE;
    m_Alloc = nullptr;
}

} // namespace DMGameEngine
