#pragma once

#include "EngineDevice.hpp"
#include <vulkan/vulkan.h>
#include <vector>

class EngineSwapChain {
public:
    EngineSwapChain(EngineDevice &deviceRef, int width, int height);
    ~EngineSwapChain();

    EngineSwapChain(const EngineSwapChain &) = delete;
    EngineSwapChain &operator=(const EngineSwapChain &) = delete;

    VkSwapchainKHR getSwapChain() { return swapchain; }
    VkRenderPass getRenderPass() { return renderPass; }
    VkFramebuffer getFrameBuffer(int index) { return swapchainFramebuffers[index]; }
    VkSemaphore getImageAvailableSemaphore() { return imageAvailableSemaphore; }
    VkSemaphore getRenderFinishedSemaphore() { return renderFinishedSemaphore; }
    VkFence getInFlightFence() { return inFlightFence; }

    VkExtent2D getSwapChainExtent() { 
        return { static_cast<uint32_t>(windowWidth), static_cast<uint32_t>(windowHeight) }; 
    }
    float extentAspectRatio() { 
        return static_cast<float>(windowWidth) / static_cast<float>(windowHeight); 
    }

    VkResult acquireNextImage(uint32_t *imageIndex);
    VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex); 
    
    int getWidth() { return windowWidth; }
    int getHeight() { return windowHeight; }

private:
    void createSwapChain();
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createSyncObjects();

    EngineDevice &device;
    int windowWidth;
    int windowHeight;

    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
};