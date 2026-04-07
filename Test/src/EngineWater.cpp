#include "EngineWater.hpp"
#include <array>
#include <stdexcept>

EngineWater::EngineWater(EngineDevice& device, uint32_t width, uint32_t height)
    : engineDevice{device}, width{width}, height{height} {
    createResources();
    createRenderPasses();
    createFramebuffers();
}

EngineWater::~EngineWater() {
    VkDevice device = engineDevice.getDevice();

    // 1. 프레임버퍼 및 렌더패스 해제
    vkDestroyFramebuffer(device, reflectionFramebuffer, nullptr);
    vkDestroyFramebuffer(device, refractionFramebuffer, nullptr);
    vkDestroyRenderPass(device, reflectionRenderPass, nullptr);
    vkDestroyRenderPass(device, refractionRenderPass, nullptr);

    // 2. 샘플러 해제
    vkDestroySampler(device, waterSampler, nullptr);

    // 3. 반사 자원 해제
    vkDestroyImageView(device, reflectionColorView, nullptr);
    vkDestroyImage(device, reflectionColorImage, nullptr);
    vkFreeMemory(device, reflectionColorMemory, nullptr);

    vkDestroyImageView(device, reflectionDepthView, nullptr);
    vkDestroyImage(device, reflectionDepthImage, nullptr);
    vkFreeMemory(device, reflectionDepthMemory, nullptr);

    // 4. 굴절 자원 해제
    vkDestroyImageView(device, refractionColorView, nullptr);
    vkDestroyImage(device, refractionColorImage, nullptr);
    vkFreeMemory(device, refractionColorMemory, nullptr);

    vkDestroyImageView(device, refractionDepthView, nullptr);
    vkDestroyImage(device, refractionDepthImage, nullptr);
    vkFreeMemory(device, refractionDepthMemory, nullptr);
}

void EngineWater::createRenderPasses() {
    // ---------------------------------------------------------
    // 공통 어태치먼트 설정 (색상과 깊이)
    // ---------------------------------------------------------
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM; // 일반적인 색상 포맷
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // 렌더링이 끝나면 셰이더에서 읽을 수 있는 텍스처(SHADER_READ_ONLY)로 자동 변환!
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // ★ 주의: 굴절은 물의 깊이를 구하기 위해 Depth값을 셰이더에서 읽어야 하므로 STORE 유지!
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 안전한 이미지 변환을 위한 의존성 설정
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    // 두 렌더패스가 구조가 똑같으므로 같은 설정으로 2개 만듭니다.
    if (vkCreateRenderPass(engineDevice.getDevice(), &renderPassInfo, nullptr, &reflectionRenderPass) != VK_SUCCESS ||
        vkCreateRenderPass(engineDevice.getDevice(), &renderPassInfo, nullptr, &refractionRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("실패: 물 렌더 패스 생성 오류!");
    }
}

void EngineWater::createResources() {
    VkFormat depthFormat = findDepthFormat();

    // 1. 반사(Reflection) 텍스처 생성
    createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, reflectionColorImage, reflectionColorMemory);
    reflectionColorView = createImageView(reflectionColorImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    createImage(width, height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, reflectionDepthImage, reflectionDepthMemory);
    reflectionDepthView = createImageView(reflectionDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    // 2. 굴절(Refraction) 텍스처 생성 (★깊이 텍스처도 셰이더에서 읽을 수 있게 SAMPLED_BIT 추가!)
    createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, refractionColorImage, refractionColorMemory);
    refractionColorView = createImageView(refractionColorImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    createImage(width, height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, refractionDepthImage, refractionDepthMemory);
    refractionDepthView = createImageView(refractionDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    // 3. 텍스처 샘플러 생성 (물 텍스처는 화면 끝에서 반복되지 않도록 CLAMP_TO_EDGE 사용)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(engineDevice.getDevice(), &samplerInfo, nullptr, &waterSampler) != VK_SUCCESS) {
        throw std::runtime_error("실패: 물 샘플러 생성 오류!");
    }
}

void EngineWater::createFramebuffers() {
    std::array<VkImageView, 2> reflectionAttachments = {reflectionColorView, reflectionDepthView};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = reflectionRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(reflectionAttachments.size());
    framebufferInfo.pAttachments = reflectionAttachments.data();
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(engineDevice.getDevice(), &framebufferInfo, nullptr, &reflectionFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("실패: 반사 프레임버퍼 생성 오류!");
    }

    std::array<VkImageView, 2> refractionAttachments = {refractionColorView, refractionDepthView};
    framebufferInfo.renderPass = refractionRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(refractionAttachments.size());
    framebufferInfo.pAttachments = refractionAttachments.data();

    if (vkCreateFramebuffer(engineDevice.getDevice(), &framebufferInfo, nullptr, &refractionFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("실패: 굴절 프레임버퍼 생성 오류!");
    }
}

// ==========================================================
// 아래는 Vulkan의 메모리 할당 및 이미지 생성을 위한 보일러플레이트 코드입니다.
// ==========================================================
void EngineWater::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(engineDevice.getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("실패: 물 이미지 생성 오류!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(engineDevice.getDevice(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("실패: 물 이미지 메모리 할당 오류!");
    }
    vkBindImageMemory(engineDevice.getDevice(), image, imageMemory, 0);
}

VkImageView EngineWater::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("실패: 물 이미지 뷰 생성 오류!");
    }
    return imageView;
}

uint32_t EngineWater::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(engineDevice.getPhysicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("실패: 적합한 메모리 유형을 찾을 수 없습니다!");
}

VkFormat EngineWater::findDepthFormat() {
    std::array<VkFormat, 3> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(engineDevice.getPhysicalDevice(), format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    throw std::runtime_error("실패: 지원되는 깊이 포맷을 찾을 수 없습니다!");
}