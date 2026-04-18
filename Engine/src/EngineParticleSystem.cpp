#include "EngineParticleSystem.hpp"
#include <stdexcept>


EngineParticleSystem::EngineParticleSystem(EngineDevice& device, EngineRenderer& renderer, EngineDescriptorManager& descriptorManager, std::vector<std::unique_ptr<EngineBuffer>>& uboBuffersMain)
    : engineDevice{device} {
    initParticles();
    createPipelines(renderer, descriptorManager, uboBuffersMain);
}

EngineParticleSystem::~EngineParticleSystem() {
    vkDestroyPipelineLayout(engineDevice.getDevice(), computePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(engineDevice.getDevice(), computeSetLayout, nullptr);
}

void EngineParticleSystem::initParticles() {
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto& particle : particles) {
        particle.position = glm::vec3(0.0f, 10.0f, 0.0f);
        particle.velocity = glm::vec3((rand() % 100 - 50) * 0.1f, (rand() % 100) * 0.1f, (rand() % 100 - 50) * 0.1f);
        particle.color = glm::vec4(1.0f, (rand() % 100) * 0.01f, 0.2f, 1.0f);
    }

    particleSSBO = std::make_unique<EngineBuffer>(
        engineDevice, sizeof(Particle) * PARTICLE_COUNT, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    particleSSBO->map();
    particleSSBO->writeToBuffer(particles.data());
}

void EngineParticleSystem::createPipelines(EngineRenderer& renderer, EngineDescriptorManager& descriptorManager, std::vector<std::unique_ptr<EngineBuffer>>& uboBuffersMain) {
    //디스크립터 세트 세팅
    std::vector<VkDescriptorSetLayoutBinding> computeBindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    computeSetLayout = descriptorManager.createDescriptorSetLayout(computeBindings);

    // 디스크립터 세트 공간 확보
    computeDescriptorSets.resize(uboBuffersMain.size());
    VkDescriptorBufferInfo ssboInfo{particleSSBO->getBuffer(), 0, VK_WHOLE_SIZE};

    // 디스크립터 세트 생성
    for (size_t i = 0; i < uboBuffersMain.size(); i++) {
        auto computeUboInfo = uboBuffersMain[i]->descriptorInfo(); 

        EngineDescriptorManager::Builder(descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, &computeUboInfo)
            .bindBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, &ssboInfo)
            .build(computeDescriptorSets[i]);
    }

    // 컴퓨트 파이프라인 생성
    VkPushConstantRange computePush{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)};
    VkPipelineLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayoutInfo.setLayoutCount = 1;
    computeLayoutInfo.pSetLayouts = &computeSetLayout;
    computeLayoutInfo.pushConstantRangeCount = 1;
    computeLayoutInfo.pPushConstantRanges = &computePush;
    vkCreatePipelineLayout(engineDevice.getDevice(), &computeLayoutInfo, nullptr, &computePipelineLayout);
    
    computePipeline = std::make_unique<EnginePipeline>(engineDevice, "../Engine/shaders/particle.comp.spv", computePipelineLayout);

    // 파이프라인 생성
    PipelineConfigInfo particleConfig{};
    EnginePipeline::defaultPipelineConfigInfo(particleConfig, 1920, 1080);
    particleConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; 
    particleConfig.attributeDescriptions.clear(); 
    particleConfig.bindingDescriptions.clear();
    particleConfig.pushConstantRanges.clear(); 
    particleConfig.renderPass = renderer.getSwapChainRenderPass();
    particleConfig.descriptorSetLayouts = {computeSetLayout}; 

    particlePipeline = std::make_unique<EnginePipeline>(engineDevice, "../Engine/shaders/particle.vert.spv", "../Engine/shaders/particle.frag.spv", particleConfig);
}


void EngineParticleSystem::computeParticles(VkCommandBuffer commandBuffer, float deltaTime, int frameIndex) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->getPipeline());

    // 디스크립터 세트 바인딩
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSets[frameIndex], 0, nullptr);
    vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &deltaTime);
    
    
    uint32_t groupCountX = (PARTICLE_COUNT + 255) / 256;
    vkCmdDispatch(commandBuffer, groupCountX, 1, 1);

    VkBufferMemoryBarrier particleBarrier{};
    particleBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    particleBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    particleBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    particleBarrier.buffer = particleSSBO->getBuffer();
    particleBarrier.offset = 0; particleBarrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 0, nullptr, 1, &particleBarrier, 0, nullptr
    );
}

void EngineParticleSystem::renderParticles(VkCommandBuffer commandBuffer, int frameIndex) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline->getPipeline());
    
    //  디스크립터 세트 바인딩
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline->getPipelineLayout(), 0, 1, &computeDescriptorSets[frameIndex], 0, nullptr);    vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);
}