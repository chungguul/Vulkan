#include "EngineDescriptorManager.hpp"
#include <stdexcept>

EngineDescriptorManager::EngineDescriptorManager(EngineDevice& device) : engineDevice{device} {
    // 1. 레이아웃(규격) 생성
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(engineDevice.getDevice(), &layoutInfo, nullptr, &globalSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("실패: 디스크립터 세트 레이아웃 생성 오류!");
    }

    // 2. 디스크립터 풀 생성
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(engineDevice.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("실패: 디스크립터 풀 생성 오류!");
    }
}

// 소멸자에서 깔끔하게 자동 해제!
EngineDescriptorManager::~EngineDescriptorManager() {
    vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(engineDevice.getDevice(), globalSetLayout, nullptr);
}

void EngineDescriptorManager::allocateGlobalDescriptorSet(VkDescriptorBufferInfo bufferInfo) {
    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = descriptorPool;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &globalSetLayout;

    if (vkAllocateDescriptorSets(engineDevice.getDevice(), &allocSetInfo, &globalDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("실패: 디스크립터 세트 할당 오류!");
    }

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = globalDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(engineDevice.getDevice(), 1, &descriptorWrite, 0, nullptr);
}