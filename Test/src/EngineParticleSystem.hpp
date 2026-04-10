#pragma once

#include "EngineDevice.hpp"
#include "EnginePipeline.hpp"
#include "EngineBuffer.hpp"
#include "EngineDescriptorManager.hpp"
#include "EngineRenderer.hpp"

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct Particle {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 velocity;
    alignas(16) glm::vec4 color;
};

class EngineParticleSystem {
public:
    static constexpr int PARTICLE_COUNT = 10000;

    EngineParticleSystem(EngineDevice& device, EngineRenderer& renderer, EngineDescriptorManager& descriptorManager, EngineBuffer& uboBufferMain);
    ~EngineParticleSystem();

    // 복사 방지
    EngineParticleSystem(const EngineParticleSystem&) = delete;
    EngineParticleSystem& operator=(const EngineParticleSystem&) = delete;

    // 파티클 위치 계산 (컴퓨트 셰이더 실행)
    void computeParticles(VkCommandBuffer commandBuffer, float frameTime);

    // 파티클 화면에 그리기
    void renderParticles(VkCommandBuffer commandBuffer);

private:
    void initParticles();
    void createPipelines(EngineRenderer& renderer, EngineDescriptorManager& descriptorManager, EngineBuffer& uboBufferMain);

    EngineDevice& engineDevice;

    std::unique_ptr<EngineBuffer> particleSSBO;
    
    VkDescriptorSetLayout computeSetLayout;
    VkDescriptorSet computeDescriptorSet;
    VkPipelineLayout computePipelineLayout;
    
    std::unique_ptr<EnginePipeline> computePipeline;
    std::unique_ptr<EnginePipeline> particlePipeline;
};