#pragma once

#include "EngineDevice.hpp"
#include "EngineSwapChain.hpp"
#include "EngineWindow.hpp"

#include <cassert>
#include <memory>

class EngineRenderer {
public:
    EngineRenderer(EngineWindow &window, EngineDevice &device);
    ~EngineRenderer();

    EngineRenderer(const EngineRenderer &) = delete;
    EngineRenderer &operator=(const EngineRenderer &) = delete;

    VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }
    float getAspectRatio() const { return swapChain->extentAspectRatio(); }
    bool isFrameInProgress() const { return isFrameStarted; }
    
    VkCommandBuffer getCurrentCommandBuffer() const {
        assert(isFrameStarted && "프레임이 시작되지 않았습니다!");
        return commandBuffer;
    }
    
    uint32_t getCurrentImageIndex() const {
        return currentImageIndex;
    }

    VkCommandBuffer beginFrame();
    void endFrame();
    
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    EngineWindow &window;
    EngineDevice &device;
    std::unique_ptr<EngineSwapChain> swapChain;

    VkCommandBuffer commandBuffer;

    uint32_t currentImageIndex{0};
    bool isFrameStarted{false};
};