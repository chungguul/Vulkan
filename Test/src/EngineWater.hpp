#pragma once

#include "EngineDevice.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

class EngineWater {
public:
    // 반사와 굴절 텍스처의 해상도 (보통 1024면 충분히 예쁩니다)
    EngineWater(EngineDevice& device, uint32_t width = 1024, uint32_t height = 1024);
    ~EngineWater();

    EngineWater(const EngineWater&) = delete;
    EngineWater& operator=(const EngineWater&) = delete;

    // --- Getter ---
    VkRenderPass getReflectionRenderPass() const { return reflectionRenderPass; }
    VkFramebuffer getReflectionFramebuffer() const { return reflectionFramebuffer; }
    VkImageView getReflectionImageView() const { return reflectionColorView; }

    VkRenderPass getRefractionRenderPass() const { return refractionRenderPass; }
    VkFramebuffer getRefractionFramebuffer() const { return refractionFramebuffer; }
    VkImageView getRefractionImageView() const { return refractionColorView; }
    VkImageView getRefractionDepthView() const { return refractionDepthView; } // 물의 깊이 계산용

    VkSampler getSampler() const { return waterSampler; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

private:
    void createRenderPasses();
    void createResources();
    void createFramebuffers();
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findDepthFormat();

    EngineDevice& engineDevice;
    uint32_t width;
    uint32_t height;

    // 반사(Reflection) 자원: 색상만 있으면 됩니다 (하늘이 비치는 거울)
    VkImage reflectionColorImage = VK_NULL_HANDLE;
    VkDeviceMemory reflectionColorMemory = VK_NULL_HANDLE;
    VkImageView reflectionColorView = VK_NULL_HANDLE;
    
    // 반사 렌더링 시 Z-buffer 역할을 할 깊이 버퍼 (텍스처로 쓰진 않음)
    VkImage reflectionDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory reflectionDepthMemory = VK_NULL_HANDLE;
    VkImageView reflectionDepthView = VK_NULL_HANDLE;

    // 굴절(Refraction) 자원: 색상과 물의 깊이를 알기 위한 깊이 맵 둘 다 필요합니다.
    VkImage refractionColorImage = VK_NULL_HANDLE;
    VkDeviceMemory refractionColorMemory = VK_NULL_HANDLE;
    VkImageView refractionColorView = VK_NULL_HANDLE;

    VkImage refractionDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory refractionDepthMemory = VK_NULL_HANDLE;
    VkImageView refractionDepthView = VK_NULL_HANDLE;

    VkSampler waterSampler = VK_NULL_HANDLE;

    VkRenderPass reflectionRenderPass = VK_NULL_HANDLE;
    VkRenderPass refractionRenderPass = VK_NULL_HANDLE;
    VkFramebuffer reflectionFramebuffer = VK_NULL_HANDLE;
    VkFramebuffer refractionFramebuffer = VK_NULL_HANDLE;
};