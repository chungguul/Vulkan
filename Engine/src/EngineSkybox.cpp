#include "EngineSkybox.hpp"
#include <stdexcept>
#include <array>

EngineSkybox::EngineSkybox(EngineDevice& device, VkRenderPass renderPass, uint32_t width, uint32_t height, 
                           EngineCubemap& cubemap, VkDescriptorSetLayout globalSetLayout, 
                           const std::vector<VkBuffer>& globalUboBuffers, VkDeviceSize uboSize)
    : engineDevice{device}, skyboxCubemap{cubemap} {
    
    uint32_t framesInFlight = static_cast<uint32_t>(globalUboBuffers.size());

    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass, width, height);
    
    createDescriptorPool(framesInFlight);
    createDescriptorSets(globalSetLayout, globalUboBuffers, uboSize);
}

EngineSkybox::~EngineSkybox() {
    vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr);
    vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
}

void EngineSkybox::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &globalSetLayout;

    if (vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("실패: 스카이박스 파이프라인 레이아웃 생성 오류!");
    }
}

void EngineSkybox::createPipeline(VkRenderPass renderPass, uint32_t width, uint32_t height) {
    PipelineConfigInfo skyboxConfig{};
    
    EnginePipeline::defaultPipelineConfigInfo(skyboxConfig, width, height); 

    skyboxConfig.attributeDescriptions.clear();
    skyboxConfig.bindingDescriptions.clear();
    skyboxConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; 
    skyboxConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;        
    skyboxConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; 

    skyboxConfig.pipelineLayout = pipelineLayout;
    skyboxConfig.renderPass = renderPass;

    pipeline = std::make_unique<EnginePipeline>(
        engineDevice, 
        "../Engine/shaders/skybox.vert.spv", 
        "../Engine/shaders/skybox.frag.spv", 
        skyboxConfig
    );
}

void EngineSkybox::createDescriptorPool(uint32_t framesInFlight) {
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, framesInFlight}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = framesInFlight;

    if (vkCreateDescriptorPool(engineDevice.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("실패: 스카이박스 디스크립터 풀 생성 오류!");
    }
}

void EngineSkybox::createDescriptorSets(VkDescriptorSetLayout globalSetLayout, const std::vector<VkBuffer>& globalUboBuffers, VkDeviceSize uboSize) {
    uint32_t framesInFlight = static_cast<uint32_t>(globalUboBuffers.size());
    descriptorSets.resize(framesInFlight);

    for (uint32_t i = 0; i < framesInFlight; i++) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool; // 멤버 변수 풀 사용!
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &globalSetLayout;

        if (vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &descriptorSets[i]) != VK_SUCCESS) {
            throw std::runtime_error("실패: 스카이박스 디스크립터 셋 할당 오류!");
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = globalUboBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = uboSize;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = skyboxCubemap.getImageView();
        imageInfo.sampler = skyboxCubemap.getSampler();

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(engineDevice.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void EngineSkybox::render(VkCommandBuffer commandBuffer, int frameIndex) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

    vkCmdBindDescriptorSets(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipelineLayout, 
        0, 1, 
        &descriptorSets[frameIndex], 
        0, nullptr
    );

    vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}