#pragma once
#include "EngineDevice.hpp"
#include "EnginePipeline.hpp"
#include "Components.hpp"
#include <entt/entt.hpp>
#include <memory>

class EngineWaterSystem {
public:
    EngineWaterSystem(EngineDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout waterSetLayout);
    ~EngineWaterSystem() = default;

    void render(VkCommandBuffer commandBuffer, entt::registry& registry, int frameIndex);
    
private:
    void createPipeline(VkRenderPass renderPass, VkDescriptorSetLayout waterSetLayout);

    EngineDevice& engineDevice;
    std::unique_ptr<EnginePipeline> pipeline;
};