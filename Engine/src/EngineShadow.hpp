#pragma once

#include "EngineDevice.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>

class EngineShadow {
public:
    EngineShadow(EngineDevice& device, uint32_t width = 2048, uint32_t height = 2048);
    ~EngineShadow();

    EngineShadow(const EngineShadow&) = delete;
    EngineShadow& operator=(const EngineShadow&) = delete;

    VkRenderPass getRenderPass() const { return shadowRenderPass; }
    VkFramebuffer getFramebuffer() const { return shadowFramebuffer; }
    VkImageView getImageView() const { return shadowImageView; }
    VkSampler getSampler() const { return shadowSampler; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

private:
    void createRenderPass();
    void createShadowResources();
    void createFramebuffer();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    EngineDevice& engineDevice;
    uint32_t width;
    uint32_t height;

    VkFormat depthFormat;
    VkImage shadowImage = VK_NULL_HANDLE;
    VkDeviceMemory shadowImageMemory = VK_NULL_HANDLE;
    VkImageView shadowImageView = VK_NULL_HANDLE;
    VkSampler shadowSampler = VK_NULL_HANDLE;

    VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;
};