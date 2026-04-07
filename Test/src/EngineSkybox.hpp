#pragma once

#include "EngineDevice.hpp"
#include "EnginePipeline.hpp"
#include "EngineCubemap.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

class EngineSkybox {
public:
    // 스왑체인의 렌더패스와 글로벌 UBO 버퍼들을 주입(Inject) 받습니다.
    EngineSkybox(EngineDevice& device, 
                 VkRenderPass renderPass,
                 uint32_t width,
                 uint32_t height,
                 EngineCubemap& cubemap, 
                 VkDescriptorSetLayout globalSetLayout,
                 const std::vector<VkBuffer>& globalUboBuffers,
                 VkDeviceSize uboSize
    ); // GlobalUbo의 크기
    ~EngineSkybox();

    EngineSkybox(const EngineSkybox&) = delete;
    EngineSkybox& operator=(const EngineSkybox&) = delete;

    // Main.cpp의 렌더링 루프 안에서 이 함수 하나만 호출하면 됩니다.
    void render(VkCommandBuffer commandBuffer, int frameIndex);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass, uint32_t width, uint32_t height);
    void createDescriptorPool(uint32_t framesInFlight);
    void createDescriptorSets(VkDescriptorSetLayout globalSetLayout, const std::vector<VkBuffer>& globalUboBuffers, VkDeviceSize uboSize);
    
    EngineDevice& engineDevice;
    EngineCubemap& skyboxCubemap;
    
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::unique_ptr<EnginePipeline> pipeline;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
};