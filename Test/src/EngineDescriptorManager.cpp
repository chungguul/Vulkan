#include "EngineDescriptorManager.hpp"
#include <stdexcept>

// ==========================================================
// EngineDescriptorManager 핵심 로직
// ==========================================================
EngineDescriptorManager::EngineDescriptorManager(EngineDevice& device) : engineDevice{device} {
    // 풀(Pool)을 아주 넉넉하게 생성해 둡니다. (1000개)
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1000;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(engineDevice.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("실패: 디스크립터 풀 생성 오류!");
    }
}

EngineDescriptorManager::~EngineDescriptorManager() {
    // 캐싱해둔 레이아웃들 일괄 파괴
    for (auto layout : layoutCache) {
        vkDestroyDescriptorSetLayout(engineDevice.getDevice(), layout, nullptr);
    }
    vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr);
}

VkDescriptorSetLayout EngineDescriptorManager::createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(engineDevice.getDevice(), &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("실패: 디스크립터 레이아웃 생성 오류!");
    }
    
    layoutCache.push_back(layout); // 삭제를 위해 보관
    return layout;
}

bool EngineDescriptorManager::allocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet& descriptorSet) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.pSetLayouts = &layout;
    allocInfo.descriptorSetCount = 1;

    if (vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS) {
        return false;
    }
    return true;
}

// ==========================================================
// Builder 기능 구현 (레고 조립)
// ==========================================================
EngineDescriptorManager::Builder::Builder(EngineDescriptorManager& manager) : manager{manager} {}

EngineDescriptorManager::Builder& EngineDescriptorManager::Builder::bindBuffer(
    uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, VkDescriptorBufferInfo* bufferInfo) {
    
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = stageFlags;
    bindings.push_back(layoutBinding);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = bufferInfo;
    writes.push_back(write);

    return *this; // 메서드 체이닝을 위해 자신을 반환
}

EngineDescriptorManager::Builder& EngineDescriptorManager::Builder::bindImage(
    uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, VkDescriptorImageInfo* imageInfo) {
    
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = stageFlags;
    bindings.push_back(layoutBinding);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pImageInfo = imageInfo;
    writes.push_back(write);

    return *this;
}

bool EngineDescriptorManager::Builder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout) {
    // 1. 등록된 바인딩 정보들로 레이아웃을 즉석에서 생성합니다.
    layout = manager.createDescriptorSetLayout(bindings);

    // 2. 생성된 레이아웃으로 세트를 할당받습니다.
    if (!manager.allocateDescriptorSet(layout, set)) {
        return false;
    }

    // 3. 할당된 세트(dstSet)를 Write 정보에 연결하고 GPU에 업데이트(전송)합니다.
    for (auto& write : writes) {
        write.dstSet = set;
    }
    vkUpdateDescriptorSets(manager.getEngineDevice().getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    return true;
}

bool EngineDescriptorManager::Builder::build(VkDescriptorSet& set) {
    VkDescriptorSetLayout dummyLayout;
    return build(set, dummyLayout);
}