#pragma once
#include "EngineDevice.hpp"
#include <vector>

class EngineDescriptorManager {
public:
    EngineDescriptorManager(EngineDevice& device);
    ~EngineDescriptorManager();

    EngineDescriptorManager(const EngineDescriptorManager&) = delete;
    EngineDescriptorManager& operator=(const EngineDescriptorManager&) = delete;

    void allocateGlobalDescriptorSet(VkDescriptorBufferInfo bufferInfo);

    VkDescriptorSetLayout getGlobalSetLayout() const { return globalSetLayout; }
    VkDescriptorSet getGlobalDescriptorSet() const { return globalDescriptorSet; }

private:
    EngineDevice& engineDevice;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet globalDescriptorSet;
};