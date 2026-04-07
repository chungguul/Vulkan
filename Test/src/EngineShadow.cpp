#include "EngineShadow.hpp"
#include <array>

EngineShadow::EngineShadow(EngineDevice& device, uint32_t width, uint32_t height)
    : engineDevice{device}, width{width}, height{height} {
    
    // 대부분의 기기에서 지원하는 고해상도 깊이 포맷
    depthFormat = VK_FORMAT_D32_SFLOAT; 

    createRenderPass();
    createShadowResources();
    createFramebuffer();
}

EngineShadow::~EngineShadow() {
    vkDestroyImageView(engineDevice.getDevice(), shadowImageView, nullptr);
    vkDestroyImage(engineDevice.getDevice(), shadowImage, nullptr);
    vkFreeMemory(engineDevice.getDevice(), shadowImageMemory, nullptr);
    vkDestroySampler(engineDevice.getDevice(), shadowSampler, nullptr);
    vkDestroyFramebuffer(engineDevice.getDevice(), shadowFramebuffer, nullptr);
    vkDestroyRenderPass(engineDevice.getDevice(), shadowRenderPass, nullptr);
}

void EngineShadow::createRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 매 프레임 그리기 전 초기화
    
    // ★ 핵심: 그림자를 메인 패스에서 읽어야 하므로 STORE_OP_STORE 로 저장해야 합니다!
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
    
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    // ★ 핵심: 그리가 끝나면 셰이더에서 텍스처로 읽을 수 있는 레이아웃으로 자동 변환됩니다.
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0; // 색상(Color)은 안 그립니다! 오직 깊이만!
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 패스 간 동기화 (그림자 그리기가 끝난 후 메인 화면에서 읽도록 보장)
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(engineDevice.getDevice(), &renderPassInfo, nullptr, &shadowRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그림자 전용 렌더패스 생성 오류!");
    }
}

void EngineShadow::createShadowResources() {
    // 1. 깊이 이미지(텍스처) 생성
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    // 그림자를 그릴 때(DEPTH_STENCIL) 쓰고, 나중에 텍스처로 읽을 때(SAMPLED) 쓴다는 뜻입니다.
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(engineDevice.getDevice(), &imageInfo, nullptr, &shadowImage) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그림자 이미지 생성 오류!");
    }

    // 2. 이미지 메모리 할당
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(engineDevice.getDevice(), shadowImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &shadowImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그림자 이미지 메모리 할당 오류!");
    }
    vkBindImageMemory(engineDevice.getDevice(), shadowImage, shadowImageMemory, 0);

    // 3. 이미지 뷰 생성
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = shadowImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &shadowImageView) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그림자 이미지 뷰 생성 오류!");
    }

    // 4. 그림자 전용 샘플러 생성
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    // ★ 핵심: 그림자 맵(카메라 시야) 바깥쪽에 있는 물체는 그림자가 안 지도록 흰색(가려지지 않음)으로 처리합니다.
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; 
    
    if (vkCreateSampler(engineDevice.getDevice(), &samplerInfo, nullptr, &shadowSampler) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그림자 샘플러 생성 오류!");
    }
}

void EngineShadow::createFramebuffer() {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = shadowRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &shadowImageView;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(engineDevice.getDevice(), &framebufferInfo, nullptr, &shadowFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("실패: 그림자 프레임버퍼 생성 오류!");
    }
}

uint32_t EngineShadow::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(engineDevice.getPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("실패: 적합한 GPU 메모리 타입을 찾을 수 없습니다!");
}