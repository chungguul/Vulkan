#pragma once
#include "EngineDevice.hpp"
#include "EnginePipeline.hpp"
#include "Components.hpp"
#include <entt/entt.hpp>
#include <memory>

class EngineShadowSystem {
public:
    EngineShadowSystem(EngineDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~EngineShadowSystem() = default;

    EngineShadowSystem(const EngineShadowSystem&) = delete;
    EngineShadowSystem& operator=(const EngineShadowSystem&) = delete;

    void render(VkCommandBuffer commandBuffer, entt::registry& registry, int frameIndex);

private:
    void createPipeline(VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);

    EngineDevice& engineDevice;
    std::unique_ptr<EnginePipeline> pipeline;
    
};