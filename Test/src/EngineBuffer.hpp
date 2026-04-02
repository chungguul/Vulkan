#pragma once
#include "EngineDevice.hpp"

class EngineBuffer {
public:
    EngineBuffer(EngineDevice& device, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~EngineBuffer();

    // 복사 방지
    EngineBuffer(const EngineBuffer&) = delete;
    EngineBuffer& operator=(const EngineBuffer&) = delete;

    void map();
    void unmap();
    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    VkBuffer getBuffer() const { return buffer; }

private:
    EngineDevice& engineDevice;
    void* mapped = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize;
};