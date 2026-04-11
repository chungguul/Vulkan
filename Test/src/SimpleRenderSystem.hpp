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
    // 생성자에서 디바이스, 렌더패스, 디스크립터 레이아웃을 받아 파이프라인을 세팅합니다.
    SimpleRenderSystem(EngineDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem &) = delete;
    SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

    // ★ 핵심 함수: 게임 로직이 넘겨준 registry(엔티티 목록)를 읽어서 화면에 그립니다.
    void renderGameObjects(VkCommandBuffer commandBuffer, entt::registry& registry, RenderPassType passType, int frameIndex);
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    EngineDevice& engineDevice;
    std::unique_ptr<EnginePipeline> enginePipeline;
    VkPipelineLayout pipelineLayout;

};