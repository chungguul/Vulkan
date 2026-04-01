#pragma once

#include "EngineDevice.hpp"
#include <vulkan/vulkan.h>
#include <vector>

class EngineSwapChain {
public:
    // 생성할 때 Device 객체와 창의 크기 정보를 받아옵니다.
    EngineSwapChain(EngineDevice &deviceRef, int width, int height);
    ~EngineSwapChain();

    EngineSwapChain(const EngineSwapChain &) = delete;
    EngineSwapChain &operator=(const EngineSwapChain &) = delete;

    // 외부에서 렌더링할 때 필요한 자원들을 가져갈 수 있도록 Getter 제공
    VkSwapchainKHR getSwapChain() { return swapchain; }
    VkRenderPass getRenderPass() { return renderPass; }
    VkFramebuffer getFrameBuffer(int index) { return swapchainFramebuffers[index]; }
    VkSemaphore getImageAvailableSemaphore() { return imageAvailableSemaphore; }
    VkSemaphore getRenderFinishedSemaphore() { return renderFinishedSemaphore; }
    VkFence getInFlightFence() { return inFlightFence; }
    
    int getWidth() { return windowWidth; }
    int getHeight() { return windowHeight; }

private:
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createSyncObjects();

    EngineDevice &device;
    int windowWidth;
    int windowHeight;

    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    // 동기화 객체 (일단 1프레임용으로 심플하게 유지합니다)
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
};