#pragma once

#include "EngineDevice.hpp"
#include "EnginePipeline.hpp"
#include "EngineCamera.hpp"
#include "Components.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <vector>

enum class RenderPassType {
    MAIN,
    REFLECTION,
    REFRACTION
};

class SimpleRenderSystem {
public:
    SimpleRenderSystem(EngineDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem &) = delete;
    SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

    void renderGameObjects(VkCommandBuffer commandBuffer, entt::registry& registry, RenderPassType passType, int frameIndex);
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    EngineDevice& engineDevice;
    std::unique_ptr<EnginePipeline> enginePipeline;
    VkPipelineLayout pipelineLayout;

};