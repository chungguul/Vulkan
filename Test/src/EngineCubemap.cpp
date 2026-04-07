#include "EngineCubemap.hpp"
#include <stdexcept>
#include <fstream>
#include <array>
#include <iostream>

EngineCubemap::EngineCubemap(EngineDevice& device, EngineTexture& hdrTexture, uint32_t resolution)
    : engineDevice{device} {
    
    // 1. HDR과 동일한 32-bit Float 포맷 사용
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    // 2. 텅 빈 6장짜리 큐브맵 캔버스 생성
    createEmptyCubemap(resolution, format);
    createCubemapImageView(format);
    createCubemapSampler();

    // 3. 컴퓨트 셰이더를 이용해 HDR 이미지를 큐브맵에 굽기 (Baking)
    convertFromHDR(hdrTexture, resolution, format);
}

EngineCubemap::~EngineCubemap() {
    vkDestroySampler(engineDevice.getDevice(), cubemapSampler, nullptr);
    vkDestroyImageView(engineDevice.getDevice(), cubemapImageView, nullptr);
    vkDestroyImage(engineDevice.getDevice(), cubemapImage, nullptr);
    vkFreeMemory(engineDevice.getDevice(), cubemapMemory, nullptr);
}

void EngineCubemap::createEmptyCubemap(uint32_t resolution, VkFormat format) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6; // ★ 큐브맵의 핵심: 6개의 면(Face)
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // ★ STORAGE_BIT: 컴퓨트 셰이더가 이 이미지에 픽셀을 직접 기록할 수 있도록 허용
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // ★ 이 이미지는 큐브맵으로 쓰일 것입니다!

    if (vkCreateImage(engineDevice.getDevice(), &imageInfo, nullptr, &cubemapImage) != VK_SUCCESS) {
        throw std::runtime_error("실패: 빈 큐브맵 이미지 생성 오류!");
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(engineDevice.getDevice(), cubemapImage, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = engineDevice.findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &cubemapMemory) != VK_SUCCESS) {
        throw std::runtime_error("실패: 큐브맵 메모리 할당 오류!");
    }
    vkBindImageMemory(engineDevice.getDevice(), cubemapImage, cubemapMemory, 0);
}

void EngineCubemap::createCubemapImageView(VkFormat format) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = cubemapImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE; // ★ 2D가 아닌 큐브맵 뷰로 생성
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6; // ★ 6장 전부 묶어서 뷰 생성

    if (vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &cubemapImageView) != VK_SUCCESS) {
        throw std::runtime_error("실패: 큐브맵 이미지 뷰 생성 오류!");
    }
}

void EngineCubemap::createCubemapSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    // 큐브맵은 경계선이 자연스럽게 이어져야 하므로 CLAMP_TO_EDGE를 사용합니다.
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(engineDevice.getDevice(), &samplerInfo, nullptr, &cubemapSampler) != VK_SUCCESS) {
        throw std::runtime_error("실패: 큐브맵 샘플러 생성 오류!");
    }
}

// 셰이더 파일 읽기 헬퍼 (기존 엔진 파이프라인에 있다면 그걸 써도 무방합니다)
std::vector<char> EngineCubemap::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("실패: 파일을 열 수 없습니다! " + filename);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

// ★ 대망의 컴퓨트 셰이더 변환 작업 ★
void EngineCubemap::convertFromHDR(EngineTexture& hdrTexture, uint32_t resolution, VkFormat format) {
    std::cout << ">>> 컴퓨트 셰이더를 이용한 큐브맵 베이킹 시작..." << std::endl;

    auto shaderCode = readFile("../Test/shaders/equirectangular_to_cubemap.spv"); // 아까 컴파일한 그 파일!
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    VkShaderModule computeShaderModule;
    vkCreateShaderModule(engineDevice.getDevice(), &createInfo, nullptr, &computeShaderModule);

    // 1. 디스크립터 셋 레이아웃 설정 (입력 HDR, 출력 큐브맵)
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(engineDevice.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout);

    // 2. 임시 디스크립터 풀 & 셋 할당
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;
    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(engineDevice.getDevice(), &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &descriptorSet);

    // 3. 디스크립터 셋에 이미지 정보 연결
    VkDescriptorImageInfo hdrInfo{};
    hdrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrInfo.imageView = hdrTexture.getImageView();
    hdrInfo.sampler = hdrTexture.getSampler();

    VkDescriptorImageInfo cubeInfo{};
    cubeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // ★ Storage Image는 무조건 GENERAL 레이아웃이어야 기록 가능합니다.
    cubeInfo.imageView = cubemapImageView;
    cubeInfo.sampler = cubemapSampler;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &hdrInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &cubeInfo;

    vkUpdateDescriptorSets(engineDevice.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    // 4. 파이프라인 생성
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShaderModule;
    pipelineInfo.stage.pName = "main";
    VkPipeline computePipeline;
    vkCreateComputePipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);

    // =========================================================
    // 5. 커맨드 버퍼 기록 및 제출 (디스패치!)
    // =========================================================
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = engineDevice.getCommandPool();
    cmdAllocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(engineDevice.getDevice(), &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // (A) 큐브맵을 쓰기 가능한 GENERAL 상태로 변경
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = cubemapImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // (B) 컴퓨트 파이프라인 바인딩 및 디스패치! (해상도 / 16(로컬 사이즈))
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, resolution / 16, resolution / 16, 6);

    // (C) 굽기가 끝난 큐브맵을 렌더링용 SHADER_READ_ONLY 상태로 변경
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    // 제출 및 대기
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(engineDevice.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(engineDevice.getGraphicsQueue());
    vkFreeCommandBuffers(engineDevice.getDevice(), engineDevice.getCommandPool(), 1, &cmd);

    // 6. 베이킹이 끝났으므로 1회성 파이프라인 자원들 전부 파기! (깔끔!)
    vkDestroyPipeline(engineDevice.getDevice(), computePipeline, nullptr);
    vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
    vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(engineDevice.getDevice(), descriptorSetLayout, nullptr);
    vkDestroyShaderModule(engineDevice.getDevice(), computeShaderModule, nullptr);

    std::cout << "<<< 큐브맵 베이킹 완료 및 임시 자원 파기 성공!" << std::endl;
}