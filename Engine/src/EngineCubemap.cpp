#include "EngineCubemap.hpp"
#include <stdexcept>
#include <fstream>
#include <array>
#include <iostream>

EngineCubemap::EngineCubemap(EngineDevice& device, EngineTexture& hdrTexture, uint32_t resolution)
    : engineDevice{device} {
    
    // HDR과 동일한 32-bit Float 포맷 사용
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    // 큐브맵 캔버스 생성
    createEmptyCubemap(resolution, format);
    createCubemapImageView(format);
    createCubemapSampler();

    // 컴퓨트 셰이더를 이용해 HDR 이미지를 큐브맵에 굽기
    convertFromHDR(hdrTexture, resolution, format);

    uint32_t irradianceRes = 32;
    createIrradianceResources(irradianceRes);
    bakeIrradianceMap(irradianceRes);

    uint32_t prefilterRes = 128;
    createPrefilteredResources(prefilterRes);
    bakePrefilteredMap(prefilterRes);
}

EngineCubemap::~EngineCubemap() {
    vkDestroyImageView(engineDevice.getDevice(), prefilteredImageView, nullptr);
    vkDestroyImage(engineDevice.getDevice(), prefilteredImage, nullptr);
    vkFreeMemory(engineDevice.getDevice(), prefilteredMemory, nullptr);

    vkDestroyImageView(engineDevice.getDevice(), irradianceImageView, nullptr);
    vkDestroyImage(engineDevice.getDevice(), irradianceImage, nullptr);
    vkFreeMemory(engineDevice.getDevice(), irradianceMemory, nullptr);

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
    imageInfo.arrayLayers = 6;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

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
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &cubemapImageView) != VK_SUCCESS) {
        throw std::runtime_error("실패: 큐브맵 이미지 뷰 생성 오류!");
    }
}

void EngineCubemap::createCubemapSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
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

void EngineCubemap::convertFromHDR(EngineTexture& hdrTexture, uint32_t resolution, VkFormat format) {
    std::cout << ">>> 컴퓨트 셰이더를 이용한 큐브맵 베이킹 시작..." << std::endl;

    auto shaderCode = readFile("../Engine/shaders/equirectangular_to_cubemap.spv"); // 아까 컴파일한 그 파일!
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    VkShaderModule computeShaderModule;
    vkCreateShaderModule(engineDevice.getDevice(), &createInfo, nullptr, &computeShaderModule);

    //디스크립터 셋 레이아웃 설정 (입력 HDR, 출력 큐브맵)
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

    //임시 디스크립터 풀 & 셋 할당
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

    //디스크립터 셋에 이미지 정보 연결
    VkDescriptorImageInfo hdrInfo{};
    hdrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrInfo.imageView = hdrTexture.getImageView();
    hdrInfo.sampler = hdrTexture.getSampler();

    VkDescriptorImageInfo cubeInfo{};
    cubeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
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

    //파이프라인 생성
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

    //커맨드 버퍼 기록 및 제출
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

    //큐브맵을 쓰기 가능한 GENERAL 상태로 변경
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

    //컴퓨트 파이프라인 바인딩 및 디스패치
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, resolution / 16, resolution / 16, 6);

    //굽기가 끝난 큐브맵을 렌더링용 SHADER_READ_ONLY 상태로 변경
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

    //베이킹이 끝났으므로 1회성 파이프라인 자원들 전부 파기
    vkDestroyPipeline(engineDevice.getDevice(), computePipeline, nullptr);
    vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
    vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(engineDevice.getDevice(), descriptorSetLayout, nullptr);
    vkDestroyShaderModule(engineDevice.getDevice(), computeShaderModule, nullptr);

    std::cout << "<<< 큐브맵 베이킹 완료 및 임시 자원 파기 성공!" << std::endl;
}

void EngineCubemap::createIrradianceResources(uint32_t resolution) {
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    //이미지 생성
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(engineDevice.getDevice(), &imageInfo, nullptr, &irradianceImage) != VK_SUCCESS) {
        throw std::runtime_error("실패: 조도 맵 이미지 생성 오류!");
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(engineDevice.getDevice(), irradianceImage, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = engineDevice.findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &irradianceMemory);
    vkBindImageMemory(engineDevice.getDevice(), irradianceImage, irradianceMemory, 0);

    //이미지 뷰 생성
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = irradianceImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &irradianceImageView); 
}

void EngineCubemap::bakeIrradianceMap(uint32_t resolution) {
    std::cout << ">>> Irradiance Map(조도 맵) 베이킹 시작..." << std::endl;

    auto shaderCode = readFile("../Engine/shaders/irradiance.comp.spv");
    
    // 셰이더 모듈, 레이아웃, 파이프라인 생성
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    VkShaderModule computeShaderModule;
    vkCreateShaderModule(engineDevice.getDevice(), &createInfo, nullptr, &computeShaderModule);

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

    VkDescriptorPoolSize poolSizes[] = { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} };
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

    //Irradiance 맵
    VkDescriptorImageInfo envInfo{};
    envInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    envInfo.imageView = cubemapImageView;
    envInfo.sampler = cubemapSampler;

    VkDescriptorImageInfo irrInfo{};
    irrInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    irrInfo.imageView = irradianceImageView;
    irrInfo.sampler = cubemapSampler;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &envInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &irrInfo;

    vkUpdateDescriptorSets(engineDevice.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

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

    // 커맨드 버퍼 기록 시작
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

    //Irradiance 이미지를 쓰기 가능하게 변경
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = irradianceImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, resolution / 16, resolution / 16, 6);

    //렌더링을 위해 SHADER_READ_ONLY로 변경
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(engineDevice.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(engineDevice.getGraphicsQueue());
    vkFreeCommandBuffers(engineDevice.getDevice(), engineDevice.getCommandPool(), 1, &cmd);

    // 임시 자원 정리
    vkDestroyPipeline(engineDevice.getDevice(), computePipeline, nullptr);
    vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
    vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(engineDevice.getDevice(), descriptorSetLayout, nullptr);
    vkDestroyShaderModule(engineDevice.getDevice(), computeShaderModule, nullptr);

    std::cout << "<<< Irradiance Map 베이킹 완료!" << std::endl;
}

void EngineCubemap::createPrefilteredResources(uint32_t resolution) {
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    // 이미지 생성
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = maxMipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(engineDevice.getDevice(), &imageInfo, nullptr, &prefilteredImage) != VK_SUCCESS) {
        throw std::runtime_error("실패: Prefiltered 이미지 생성 오류!");
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(engineDevice.getDevice(), prefilteredImage, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = engineDevice.findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &prefilteredMemory);
    vkBindImageMemory(engineDevice.getDevice(), prefilteredImage, prefilteredMemory, 0);

    //메인 이미지 뷰 생성
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = prefilteredImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = maxMipLevels; // ★ 5단계 전체 포함
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &prefilteredImageView);
}

void EngineCubemap::bakePrefilteredMap(uint32_t resolution) {
    std::cout << ">>> Prefiltered Map(사전 필터링 맵) 다중 밉맵 베이킹 시작..." << std::endl;

    auto shaderCode = readFile("../Engine/shaders/prefilter.comp.spv");
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    VkShaderModule computeShaderModule;
    vkCreateShaderModule(engineDevice.getDevice(), &createInfo, nullptr, &computeShaderModule);

    // 디스크립터 및 레이아웃 설정
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0; bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; bindings[0].descriptorCount = 1; bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1; bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; bindings[1].descriptorCount = 1; bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size()); layoutInfo.pBindings = bindings.data();
    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(engineDevice.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout);

    //푸시 상수(Push Constant) 설정
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1; pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pipelineInfo.stage.module = computeShaderModule; pipelineInfo.stage.pName = "main";
    VkPipeline computePipeline;
    vkCreateComputePipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);

    // 디스크립터 풀 생성
    VkDescriptorPoolSize poolSizes[] = { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxMipLevels} };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; poolInfo.poolSizeCount = 2; poolInfo.pPoolSizes = poolSizes; poolInfo.maxSets = maxMipLevels;
    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(engineDevice.getDevice(), &poolInfo, nullptr, &descriptorPool);

    // 커맨드 버퍼 준비
    VkCommandBufferAllocateInfo cmdAllocInfo{}; cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cmdAllocInfo.commandPool = engineDevice.getCommandPool(); cmdAllocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd; vkAllocateCommandBuffers(engineDevice.getDevice(), &cmdAllocInfo, &cmd);
    VkCommandBufferBeginInfo beginInfo{}; beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    //전체 이미지를 GENERAL 레이아웃으로 변경
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = prefilteredImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0; barrier.subresourceRange.levelCount = maxMipLevels;
    barrier.subresourceRange.baseArrayLayer = 0; barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0; barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);


    // 각 밉맵 레벨마다 거칠기를 올려가며 baking

    for (uint32_t mip = 0; mip < maxMipLevels; mip++) {
        // 현재 밉맵 해상도 계산 (128 -> 64 -> 32 -> 16 -> 8)
        uint32_t mipWidth = resolution * std::pow(0.5, mip);
        uint32_t mipHeight = resolution * std::pow(0.5, mip);
        float roughness = (float)mip / (float)(maxMipLevels - 1);

        // 특정 밉맵 1장만을 가리키는 전용 임시 이미지 뷰 생성
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = prefilteredImage; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE; viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = mip; viewInfo.subresourceRange.levelCount = 1; // ★ 오직 이 밉맵만!
        viewInfo.subresourceRange.baseArrayLayer = 0; viewInfo.subresourceRange.layerCount = 6;
        VkImageView mipView;
        vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &mipView);

        // 디스크립터 셋 할당 및 업데이트
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1; allocInfo.pSetLayouts = &descriptorSetLayout;
        VkDescriptorSet descriptorSet; vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &descriptorSet);

        VkDescriptorImageInfo envInfo{}; envInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; envInfo.imageView = cubemapImageView; envInfo.sampler = cubemapSampler;
        VkDescriptorImageInfo prefInfo{}; prefInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; prefInfo.imageView = mipView; prefInfo.sampler = cubemapSampler;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[0].dstSet = descriptorSet; writes[0].dstBinding = 0; writes[0].dstArrayElement = 0; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[0].descriptorCount = 1; writes[0].pImageInfo = &envInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[1].dstSet = descriptorSet; writes[1].dstBinding = 1; writes[1].dstArrayElement = 0; writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; writes[1].descriptorCount = 1; writes[1].pImageInfo = &prefInfo;
        vkUpdateDescriptorSets(engineDevice.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        // 셰이더 바인딩 및 푸시 상수 전달, 디스패치
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &roughness);
        
        // 워크그룹 개수는 해상도에 비례해서 줄어듬
        uint32_t groupX = std::max(1u, mipWidth / 16);
        vkCmdDispatch(cmd, groupX, groupX, 6);

        // 임시 뷰는 렌더링 끝난 후 삭제해야 하므로 큐에 보관
    }

    //모든 밉맵 베이킹 후, 전체 이미지를 렌더링 읽기 전용으로 변환
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    // 실행 및 대기
    VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(engineDevice.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(engineDevice.getGraphicsQueue());
    vkFreeCommandBuffers(engineDevice.getDevice(), engineDevice.getCommandPool(), 1, &cmd);

    // 임시 자원 정리
    vkDestroyPipeline(engineDevice.getDevice(), computePipeline, nullptr); vkDestroyPipelineLayout(engineDevice.getDevice(), pipelineLayout, nullptr);
    vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr); vkDestroyDescriptorSetLayout(engineDevice.getDevice(), descriptorSetLayout, nullptr); vkDestroyShaderModule(engineDevice.getDevice(), computeShaderModule, nullptr);

    std::cout << "<<< Prefiltered Map 다중 밉맵 베이킹 완료!" << std::endl;
}