#include "EngineShadowSystem.hpp"

EngineShadowSystem::EngineShadowSystem(EngineDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : engineDevice{device} {
    createPipeline(renderPass, globalSetLayout);
}

void EngineShadowSystem::createPipeline(VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    PipelineConfigInfo pipelineConfig{};
    EnginePipeline::defaultPipelineConfigInfo(pipelineConfig, 2048, 2048); // 그림자 맵 해상도
    pipelineConfig.colorBlendInfo.attachmentCount = 0;
    pipelineConfig.colorBlendInfo.pAttachments = nullptr;
    pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT; // 그림자용 프론트 컬링
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

    pipelineConfig.rasterizationInfo.depthBiasEnable = VK_TRUE;
    pipelineConfig.rasterizationInfo.depthBiasConstantFactor = 1.25f;
    pipelineConfig.rasterizationInfo.depthBiasSlopeFactor = 1.75f;

    pipelineConfig.renderPass = renderPass;
    pipelineConfig.descriptorSetLayouts = {globalSetLayout};
    pipelineConfig.pushConstantRanges = {pushConstantRange};

    pipeline = std::make_unique<EnginePipeline>(engineDevice, "../Test/shaders/shadow.vert.spv", "../Test/shaders/shadow.frag.spv", pipelineConfig);
}

void EngineShadowSystem::render(VkCommandBuffer commandBuffer, entt::registry& registry, int frameIndex) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

    auto view = registry.view<TransformComponent, ModelComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& modelComp = view.get<ModelComponent>(entity);

        SimplePushConstantData push{};
        push.modelMatrix = transform.mat4();

        push.characterIndex = 0; // 정적 사물은 기본값 0
        if (registry.all_of<AnimatorComponent>(entity)) {
            push.characterIndex = registry.get<AnimatorComponent>(entity).characterIndex;
        }

        vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &modelComp.mainSets[frameIndex], 0, nullptr);
        modelComp.model->bind(commandBuffer);
        modelComp.model->draw(commandBuffer);
    }
}