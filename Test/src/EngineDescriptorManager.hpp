#pragma once
#include "EngineDevice.hpp"
#include <vector>

class EngineDescriptorManager {
public:
    EngineDescriptorManager(EngineDevice& device);
    ~EngineDescriptorManager();

    EngineDescriptorManager(const EngineDescriptorManager&) = delete;
    EngineDescriptorManager& operator=(const EngineDescriptorManager&) = delete;

    VkDescriptorSet allocateDescriptorSet(VkDescriptorBufferInfo bufferInfo, VkDescriptorImageInfo imageInfo, VkDescriptorImageInfo shadowImageInfo);

    VkDescriptorSetLayout getGlobalSetLayout() const { return globalSetLayout; }
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }

private:
    EngineDevice& engineDevice;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorPool descriptorPool;
};