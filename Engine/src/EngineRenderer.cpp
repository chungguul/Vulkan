#include "EngineRenderer.hpp"
#include <stdexcept>
#include <array>

EngineRenderer::EngineRenderer(EngineWindow &window, EngineDevice &device)
    : window{window}, device{device} {
    recreateSwapChain();
    createCommandBuffers();
}

EngineRenderer::~EngineRenderer() {
    freeCommandBuffers();
}

void EngineRenderer::recreateSwapChain() {
    auto extent = window.getExtent();
    while (extent.width == 0 || extent.height == 0) {
        extent = window.getExtent();
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(device.getDevice());

    if (swapChain == nullptr) {
        swapChain = std::make_unique<EngineSwapChain>(device, extent.width, extent.height);
    } else {
        swapChain.reset();
        swapChain = std::make_unique<EngineSwapChain>(device, extent.width, extent.height);
    }
}

void EngineRenderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device.getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("커맨드 버퍼 할당 실패!");
    }
}

void EngineRenderer::freeCommandBuffers() {
    vkFreeCommandBuffers(device.getDevice(), device.getCommandPool(), 1, &commandBuffer);
}

VkCommandBuffer EngineRenderer::beginFrame() {
    assert(!isFrameStarted && "이미 프레임이 진행 중입니다!");

    auto result = swapChain->acquireNextImage(&currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return nullptr;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("스왑체인 이미지 획득 실패!");
    }

    isFrameStarted = true;

    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("커맨드 버퍼 기록 시작 실패!");
    }
    return commandBuffer;
}

void EngineRenderer::endFrame() {
    assert(isFrameStarted && "프레임이 시작되지 않았습니다!");

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("커맨드 버퍼 기록 종료 실패!");
    }

    auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized()) {
        window.resetWindowResizedFlag();
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("커맨드 버퍼 제출 실패!");
    }

    isFrameStarted = false;
}

void EngineRenderer::beginSwapChainRenderPass(VkCommandBuffer cmdBuffer) {
    assert(isFrameStarted && "프레임 진행 중에만 렌더 패스를 시작할 수 있습니다!");

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = swapChain->getRenderPass();
    renderPassInfo.framebuffer = swapChain->getFrameBuffer(currentImageIndex);

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->getSwapChainExtent();

    VkClearValue depthClear{}; 
    depthClear.depthStencil = {1.0f, 0};
    VkClearValue clearColor = {{{0.02f, 0.05f, 0.1f, 1.0f}}};
    std::array<VkClearValue, 2> clearValues{clearColor, depthClear};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f; viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChain->getWidth());
    viewport.height = static_cast<float>(swapChain->getHeight());
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, swapChain->getSwapChainExtent()};
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
}

void EngineRenderer::endSwapChainRenderPass(VkCommandBuffer cmdBuffer) {
    assert(isFrameStarted && "렌더 패스가 진행 중이 아닙니다!");
    vkCmdEndRenderPass(cmdBuffer);
}