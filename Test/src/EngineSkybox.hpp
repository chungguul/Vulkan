#pragma once

#include "EngineDevice.hpp"
#include "EnginePipeline.hpp"
#include "EngineCubemap.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

class EngineSkybox {
public:
    EngineSkybox(EngineDevice& device, 
                 VkRenderPass renderPass,
                 uint32_t width,
                 uint32_t height,
                 EngineCubemap& cubemap, 
                 VkDescriptorSetLayout globalSetLayout,
                 const std::vector<VkBuffer>& globalUboBuffers,
                 VkDeviceSize uboSize
    );
    ~EngineSkybox();

    EngineSkybox(const EngineSkybox&) = delete;
    EngineSkybox& operator=(const EngineSkybox&) = delete;

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