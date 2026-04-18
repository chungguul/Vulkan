#include "EnginePipeline.hpp"
#include "EngineModel.hpp" // Vertex 정보를 가져오기 위해 필요
#include <fstream>
#include <iostream>
#include <stdexcept>

EnginePipeline::EnginePipeline(EngineDevice& device, const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo)
    : engineDevice{device} {
    createGraphicsPipeline(vertFilepath, fragFilepath, configInfo);
}

EnginePipeline::EnginePipeline(EngineDevice &device, const std::string &computeFilepath, VkPipelineLayout pipelineLayout)
    : engineDevice{device} 
{
    createComputePipeline(computeFilepath, pipelineLayout);
}

EnginePipeline::~EnginePipeline() {
    vkDestroyPipeline(engineDevice.getDevice(), graphicsPipeline, nullptr);
    
    // 이 클래스가 레이아웃을 직접 생성했을 때만 메모리 해제
    if (ownsPipelineLayout) {
        vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
    }
}


void EnginePipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo, uint32_t width, uint32_t height) {
    // 버텍스 입력
    configInfo.bindingDescriptions = {Vertex::getBindingDescription()};
    configInfo.attributeDescriptions = Vertex::getAttributeDescriptions();

    // 뷰포트, 가위
    configInfo.viewport.x = 0.0f;
    configInfo.viewport.y = 0.0f;
    configInfo.viewport.width = static_cast<float>(width);
    configInfo.viewport.height = static_cast<float>(height);
    configInfo.viewport.minDepth = 0.0f;
    configInfo.viewport.maxDepth = 1.0f;

    configInfo.scissor.offset = {0, 0};
    configInfo.scissor.extent = {width, height};

    configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    configInfo.viewportInfo.viewportCount = 1;
    configInfo.viewportInfo.pViewports = &configInfo.viewport;
    configInfo.viewportInfo.scissorCount = 1;
    configInfo.viewportInfo.pScissors = &configInfo.scissor;

    // 입력 어셈블리
    configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    // 래스터라이저
    configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
    configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    configInfo.rasterizationInfo.lineWidth = 1.0f;
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT; 
    configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // 멀티샘플링
    configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
    configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 컬러 블렌딩
    configInfo.colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    configInfo.colorBlendAttachment.blendEnable = VK_FALSE;

    configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
    configInfo.colorBlendInfo.attachmentCount = 1;
    configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;

    // 깊이 버퍼 
    configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
}

std::vector<char> EnginePipeline::readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("실패: 셰이더 파일을 열 수 없습니다! 경로: " + filepath);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0); file.read(buffer.data(), fileSize); file.close();
    return buffer;
}

VkShaderModule EnginePipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(engineDevice.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("실패: 셰이더 모듈 생성 오류!");
    return shaderModule;
}


void EnginePipeline::createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo) {
    auto vertCode = readFile(vertFilepath);
    auto fragCode = readFile(fragFilepath);

    VkShaderModule vertShaderModule = createShaderModule(vertCode);
    VkShaderModule fragShaderModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main", nullptr}
    };

    // 버텍스 입력 정보 조립
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(configInfo.attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = configInfo.attributeDescriptions.data();
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(configInfo.bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = configInfo.bindingDescriptions.data();

    // 외부에서 레이아웃을 주입했는지 검사
    if (configInfo.pipelineLayout == VK_NULL_HANDLE) {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(configInfo.descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = configInfo.descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(configInfo.pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = configInfo.pushConstantRanges.data();

        if (vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("실패: 파이프라인 레이아웃 생성 오류!");
        }
        ownsPipelineLayout = true;
    } else {
        pipelineLayout = configInfo.pipelineLayout;
        ownsPipelineLayout = false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
    pipelineInfo.pViewportState = &configInfo.viewportInfo;
    pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
    pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
    pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
    pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = configInfo.renderPass;
    pipelineInfo.subpass = configInfo.subpass;

    if (vkCreateGraphicsPipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그래픽스 파이프라인 생성 오류!");
    }

    vkDestroyShaderModule(engineDevice.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(engineDevice.getDevice(), vertShaderModule, nullptr);
}

void EnginePipeline::createComputePipeline(const std::string &computeFilepath, VkPipelineLayout pipelineLayout) {
    auto computeCode = readFile(computeFilepath);
    VkShaderModule computeShaderModule = createShaderModule(computeCode);

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("컴퓨트 파이프라인 생성 실패!");
    }

    vkDestroyShaderModule(engineDevice.getDevice(), computeShaderModule, nullptr);
    
    this->pipelineLayout = pipelineLayout;
    this->ownsPipelineLayout = false;
}