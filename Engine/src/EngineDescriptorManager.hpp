#pragma once
#include "EngineDevice.hpp"
#include <vector>
#include <memory>

class EngineDescriptorManager {
public:
    EngineDescriptorManager(EngineDevice& device);
    ~EngineDescriptorManager();

    EngineDescriptorManager(const EngineDescriptorManager&) = delete;
    EngineDescriptorManager& operator=(const EngineDescriptorManager&) = delete;

    // 즉석에서 레이아웃을 생성하고 보관하는 함수
    VkDescriptorSetLayout createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    
    // 디스크립터 세트 할당
    bool allocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet& descriptorSet);

    EngineDevice& getEngineDevice() { return engineDevice; }

    class Builder {
    public:
        Builder(EngineDescriptorManager& manager);

        Builder& bindBuffer(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, VkDescriptorBufferInfo* bufferInfo);
        Builder& bindImage(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, VkDescriptorImageInfo* imageInfo);

        bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
        
        bool build(VkDescriptorSet& set); 

    private:
        EngineDescriptorManager& manager;
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkWriteDescriptorSet> writes;
    };

private:
    EngineDevice& engineDevice;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSetLayout> layoutCache;
};