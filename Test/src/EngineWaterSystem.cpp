#include "EngineWaterSystem.hpp"

EngineWaterSystem::EngineWaterSystem(EngineDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout waterSetLayout)
    : engineDevice{device} {
    createPipeline(renderPass, waterSetLayout);
}

void EngineWaterSystem::createPipeline(VkRenderPass renderPass, VkDescriptorSetLayout waterSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    PipelineConfigInfo pipelineConfig{};
    EnginePipeline::defaultPipelineConfigInfo(pipelineConfig, 1920, 1080);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.descriptorSetLayouts = {waterSetLayout};
    pipelineConfig.pushConstantRanges = {pushConstantRange};
    pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; // 물은 양면 렌더링

    pipeline = std::make_unique<EnginePipeline>(engineDevice, "../Test/shaders/water.vert.spv", "../Test/shaders/water.frag.spv", pipelineConfig);
}

void EngineWaterSystem::render(VkCommandBuffer commandBuffer, entt::registry& registry, int frameIndex) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

    auto view = registry.view<WaterComponent, TransformComponent, ModelComponent>();
    for (auto entity : view) {
        auto& water = view.get<WaterComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        auto& modelComp = view.get<ModelComponent>(entity);

        SimplePushConstantData push{};
        push.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, water.height, 0.0f)) * transform.mat4();

        push.characterIndex = 0; // 정적 사물은 기본값 0
        if (registry.all_of<AnimatorComponent>(entity)) {
            push.characterIndex = registry.get<AnimatorComponent>(entity).characterIndex;
        }

        vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);
        
        // ★ 여기서 water.waterSet 을 사용합니다! (각자의 고유 텍스처 세트)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &water.waterSets[frameIndex], 0, nullptr);

        modelComp.model->bind(commandBuffer);
        modelComp.model->draw(commandBuffer);
    }
}