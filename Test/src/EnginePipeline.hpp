#pragma once

#include "EngineDevice.hpp"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class EnginePipeline {
public:
    // 렌더 패스와 화면 크기 정보를 추가로 받습니다.
    EnginePipeline(EngineDevice& device, const std::string& vertFilepath, const std::string& fragFilepath, VkRenderPass renderPass, uint32_t width, uint32_t height);
    ~EnginePipeline();

    // 외부에서 파이프라인을 바인딩할 수 있도록 Getter 제공
    VkPipeline getPipeline() { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() { return pipelineLayout; }

private:
    static std::vector<char> readFile(const std::string& filepath);
    void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, VkRenderPass renderPass, uint32_t width, uint32_t height);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    EngineDevice& engineDevice;
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
};