#include "SimpleRenderSystem.hpp"
#include <stdexcept>
#include <iostream>

SimpleRenderSystem::SimpleRenderSystem(EngineDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : engineDevice{device} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
    vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
}

void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("파이프라인 레이아웃 생성 실패!");
    }
}

void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo pipelineConfig{};
    EnginePipeline::defaultPipelineConfigInfo(pipelineConfig, 1920, 1080); // 임시 해상도
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    enginePipeline = std::make_unique<EnginePipeline>(
        engineDevice,
        "../Test/shaders/vert.spv",
        "../Test/shaders/frag.spv",
        pipelineConfig);
}

void SimpleRenderSystem::renderGameObjects(VkCommandBuffer commandBuffer, entt::registry& registry, RenderPassType passType, int frameIndex) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, enginePipeline->getPipeline());

    auto view = registry.view<TransformComponent, ModelComponent, MaterialComponent>(entt::exclude<WaterComponent>);
    for (auto entity : view) {

        if (registry.any_of<CullingComponent>(entity)) {
            auto& cull = registry.get<CullingComponent>(entity);
            if (!cull.isVisible) continue; 
        }

        auto &transform = view.get<TransformComponent>(entity);
        auto &modelComp = view.get<ModelComponent>(entity);

        SimplePushConstantData push{};
        push.modelMatrix = transform.mat4();
        push.roughness = modelComp.roughness;
        push.metallic = modelComp.metallic;

        push.characterIndex = 0; 
        if (registry.all_of<AnimatorComponent>(entity)) {
            push.characterIndex = registry.get<AnimatorComponent>(entity).characterIndex;
        }

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);                
        
        // =======================================================
        // ★ [수정됨] 단일 세트가 아닌, 다중 프레임 배열에서 [frameIndex]로 꺼내옵니다!
        // =======================================================
        VkDescriptorSet setToBind = VK_NULL_HANDLE;
        if (passType == RenderPassType::MAIN) {
            setToBind = modelComp.mainSets[frameIndex];       // ★ mainSets 배열
        } else if (passType == RenderPassType::REFLECTION) {
            setToBind = modelComp.reflectionSets[frameIndex]; // ★ reflectionSets 배열
        } else if (passType == RenderPassType::REFRACTION) {
            setToBind = modelComp.refractionSets[frameIndex]; // ★ refractionSets 배열
        }

        // 고른 세트를 바인딩!
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &setToBind, 0, nullptr);

        modelComp.model->bind(commandBuffer);
        modelComp.model->draw(commandBuffer);
    }
}