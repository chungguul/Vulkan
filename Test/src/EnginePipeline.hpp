#pragma once

#include "EngineDevice.hpp"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class EnginePipeline {
public:
    // 생성자 맨 끝에 pushConstantRanges 배열을 받을 수 있도록 추가합니다.
    EnginePipeline(
        EngineDevice& device, 
        const std::string& vertFilepath, 
        const std::string& fragFilepath, 
        VkRenderPass renderPass, 
        uint32_t width, uint32_t height,
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<VkPushConstantRange>& pushConstantRanges = {}); // 기본값은 빈 배열
    
    ~EnginePipeline();

    VkPipeline getPipeline() { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() { return pipelineLayout; }

private:
    static std::vector<char> readFile(const std::string& filepath);
    
    void createGraphicsPipeline(
        const std::string& vertFilepath, 
        const std::string& fragFilepath, 
        VkRenderPass renderPass, 
        uint32_t width, uint32_t height, 
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<VkPushConstantRange>& pushConstantRanges);
        
    VkShaderModule createShaderModule(const std::vector<char>& code);

    EngineDevice& engineDevice;
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
};