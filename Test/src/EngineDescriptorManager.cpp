#include "EngineDescriptorManager.hpp"
#include <stdexcept>

EngineDescriptorManager::EngineDescriptorManager(EngineDevice& device) : engineDevice{device} {
    // 1. 레이아웃(규격) 생성
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // 1.5 텍스처 샘플러 레이아웃 (Binding 1)
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // 프래그먼트에서만 사용

    std::vector<VkDescriptorSetLayoutBinding> bindings = {uboLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u_int32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(engineDevice.getDevice(), &layoutInfo, nullptr, &globalSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("실패: 디스크립터 세트 레이아웃 생성 오류!");
    }

    // 2. 디스크립터 풀 생성
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u_int32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
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

void EngineDescriptorManager::allocateGlobalDescriptorSet(VkDescriptorBufferInfo bufferInfo, VkDescriptorImageInfo imageInfo) {
    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = descriptorPool;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &globalSetLayout;

    if (vkAllocateDescriptorSets(engineDevice.getDevice(), &allocSetInfo, &globalDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("실패: 디스크립터 세트 할당 오류!");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites(2);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = globalDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = globalDescriptorSet;
    descriptorWrites[1].dstBinding = 1; // 1번 바인딩
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo; // 이미지 정보 연결

    vkUpdateDescriptorSets(engineDevice.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

}