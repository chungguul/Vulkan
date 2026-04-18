#pragma once

#include "EngineDevice.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

class EngineWater {
public:
    EngineWater(EngineDevice& device, uint32_t width = 1024, uint32_t height = 1024);
    ~EngineWater();

    EngineWater(const EngineWater&) = delete;
    EngineWater& operator=(const EngineWater&) = delete;

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

    VkImage reflectionColorImage = VK_NULL_HANDLE;
    VkDeviceMemory reflectionColorMemory = VK_NULL_HANDLE;
    VkImageView reflectionColorView = VK_NULL_HANDLE;
    
    VkImage reflectionDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory reflectionDepthMemory = VK_NULL_HANDLE;
    VkImageView reflectionDepthView = VK_NULL_HANDLE;

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