#pragma once

#include "EngineDevice.hpp"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct PipelineConfigInfo {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    VkPipelineViewportStateCreateInfo viewportInfo{};
    VkViewport viewport{};
    VkRect2D scissor{};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{};
    std::vector<VkPushConstantRange> pushConstantRanges{};
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
};


class EnginePipeline {
public:
    // 생성자 맨 끝에 pushConstantRanges 배열을 받을 수 있도록 추가합니다.
    EnginePipeline(
            EngineDevice& device, 
            const std::string& vertFilepath, 
            const std::string& fragFilepath, 
            const PipelineConfigInfo& configInfo
    );

    EnginePipeline(EngineDevice &device, const std::string &computeFilepath, VkPipelineLayout pipelineLayout);
    
    ~EnginePipeline();

    VkPipeline getPipeline() { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() { return pipelineLayout; }

    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo, uint32_t width, uint32_t height);

private:
    static std::vector<char> readFile(const std::string& filepath);
    
    void createGraphicsPipeline(
        const std::string& vertFilepath, 
        const std::string& fragFilepath, 
        const PipelineConfigInfo& configInfo
    );
        
void createComputePipeline(const std::string &computeFilepath, VkPipelineLayout pipelineLayout);

    VkShaderModule createShaderModule(const std::vector<char>& code);

    EngineDevice& engineDevice;
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;

    bool ownsPipelineLayout = false;

};