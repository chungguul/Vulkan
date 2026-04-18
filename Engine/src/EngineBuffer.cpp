#include "EngineBuffer.hpp"
#include <cstring>
#include <stdexcept>

EngineBuffer::EngineBuffer(EngineDevice& device, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : engineDevice{device}, bufferSize{bufferSize} {

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(engineDevice.getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("실패: 버퍼 생성 오류!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engineDevice.getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = engineDevice.findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("실패: 버퍼 메모리 할당 오류!");
    }
    vkBindBufferMemory(engineDevice.getDevice(), buffer, memory, 0);
}

EngineBuffer::~EngineBuffer() {
    unmap();
    vkDestroyBuffer(engineDevice.getDevice(), buffer, nullptr);
    vkFreeMemory(engineDevice.getDevice(), memory, nullptr);
}

void EngineBuffer::map() {
    if (!mapped) {
        vkMapMemory(engineDevice.getDevice(), memory, 0, bufferSize, 0, &mapped);
    }
}

void EngineBuffer::unmap() {
    if (mapped) {
        vkUnmapMemory(engineDevice.getDevice(), memory);
        mapped = nullptr;
    }
}

void EngineBuffer::writeToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (size == VK_WHOLE_SIZE) size = bufferSize;
    if (mapped) {
        memcpy(static_cast<char*>(mapped) + offset, data, size);
    } else {
        map();
        memcpy(static_cast<char*>(mapped) + offset, data, size);
        unmap();
    }
}

VkDescriptorBufferInfo EngineBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) {
    if (size == VK_WHOLE_SIZE) size = bufferSize;
    return VkDescriptorBufferInfo{buffer, offset, size};
}