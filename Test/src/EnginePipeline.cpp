#include "EnginePipeline.hpp"
#include "EngineModel.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

EnginePipeline::EnginePipeline(EngineDevice& device, const std::string& vertFilepath, const std::string& fragFilepath, VkRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, const std::vector<VkPushConstantRange>& pushConstantRanges)
    : engineDevice{device} {
    createGraphicsPipeline(vertFilepath, fragFilepath, renderPass, width, height, descriptorSetLayouts, pushConstantRanges);
}

EnginePipeline::~EnginePipeline() {
    // 생성한 파이프라인과 레이아웃은 프로그램 종료 시 반드시 파괴해야 합니다.
    vkDestroyPipeline(engineDevice.getDevice(), graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
}

std::vector<char> EnginePipeline::readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("실패: 셰이더 파일을 열 수 없습니다! 경로: " + filepath);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

// 읽어들인 바이트 코드를 Vulkan이 사용할 수 있는 셰이더 모듈 객체로 변환합니다.
VkShaderModule EnginePipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(engineDevice.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("실패: 셰이더 모듈 생성 오류!");
    }
    return shaderModule;
}


void EnginePipeline::createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, VkRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, const std::vector<VkPushConstantRange>& pushConstantRanges) {
    auto vertCode = readFile(vertFilepath);
    auto fragCode = readFile(fragFilepath);

    VkShaderModule vertShaderModule = createShaderModule(vertCode);
    VkShaderModule fragShaderModule = createShaderModule(fragCode);

    // 1. 셰이더 스테이지 설정
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main"; // 셰이더 안의 main 함수 호출

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // 2. 버텍스 입력 (EngineModel에서 가져온 설명서 장착)
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 3. 입력 어셈블리 (점들을 이어서 삼각형으로 만듭니다)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 4. 뷰포트 및 가위(Scissor) 설정 (화면의 어느 부분에 그릴지 결정)
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)width;
    viewport.height = (float)height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // 5. 래스터라이저 (도형을 픽셀로 변환. 선으로 그릴지, 면으로 채울지 설정)
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // 안을 채움
    rasterizer.lineWidth = 1.0f;
    // 뒷면을 안 그리는 컬링(Culling) 설정. 지금은 안전하게 끕니다.
    rasterizer.cullMode = VK_CULL_MODE_NONE; 
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // 6. 멀티샘플링 (안티앨리어싱 - 지금은 꺼둡니다)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 7. 컬러 블렌딩 (픽셀 색상을 어떻게 섞을지 설정 - 덮어쓰기로 설정)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 7.5 깊이 버퍼 설정 (Depth Stencil)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;       // 깊이 테스트 켜기
    depthStencil.depthWriteEnable = VK_TRUE;      // 깊이 버퍼에 쓰기 켜기
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // 더 가까운(값이 작은) 픽셀만 그리기
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 8. 파이프라인 레이아웃 (셰이더에 유니폼 변수 같은 전역 값을 넘길 때 사용. 일단 비워둠)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    //디스크립터 레이아웃(규격)을 파이프라인에 연결합니다.
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

    // 외부에서 받아온 푸시 상수 정보를 여기에 넣어줍니다.
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

    if (vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("실패: 파이프라인 레이아웃 생성 오류!");
    }

    // --- 대망의 파이프라인 생성 ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass; // 이 파이프라인이 어떤 렌더 패스에서 쓰일지 명시
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그래픽스 파이프라인 생성 오류!");
    }
    std::cout << "성공: 그래픽스 파이프라인 생성 완료!" << std::endl;

    // 파이프라인이 구워졌으므로 셰이더 모듈은 더 이상 필요 없으니 지워줍니다.
    vkDestroyShaderModule(engineDevice.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(engineDevice.getDevice(), vertShaderModule, nullptr);
}