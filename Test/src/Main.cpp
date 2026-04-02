#include "EngineWindow.hpp"
#include "EngineDevice.hpp"
#include "EngineSwapChain.hpp"
#include "EnginePipeline.hpp"
#include "EngineModel.hpp"
#include "EngineGameObject.hpp"
#include "EngineCamera.hpp" // 카메라 추가!
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>

// 이제 셰이더는 4x4 결합 행렬 하나만 받습니다!
struct SimplePushConstantData {
    glm::mat4 transform{1.0f}; 
};

const int WIDTH = 800;
const int HEIGHT = 600;

int main() {
    EngineWindow window{WIDTH, HEIGHT, "Vulkan Engine"};
    EngineDevice device{window};
    EngineSwapChain swapChain{device, WIDTH, HEIGHT};

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; 
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    EnginePipeline pipeline{device, "../Test/shaders/vert.spv", "../Test/shaders/frag.spv", swapChain.getRenderPass(), WIDTH, HEIGHT, {pushConstantRange}};

    // 1. Z축 데이터(0.0f)가 추가된 3D 정점 배열
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}
    };
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    auto quadModel = std::make_shared<EngineModel>(device, vertices, indices);

    std::vector<EngineGameObject> gameObjects;

    auto obj1 = EngineGameObject::createGameObject();
    obj1.model = quadModel;
    obj1.transform.translation = {-0.5f, 0.0f, 0.0f}; 
    obj1.transform.scale = {0.5f, 0.5f, 0.5f}; 
    gameObjects.push_back(std::move(obj1));

    auto obj2 = EngineGameObject::createGameObject();
    obj2.model = quadModel;
    obj2.transform.translation = {0.5f, 0.0f, 0.0f}; 
    obj2.transform.scale = {0.8f, 0.8f, 0.8f}; 
    gameObjects.push_back(std::move(obj2));

    auto obj3 = EngineGameObject::createGameObject();
    obj3.model = quadModel;
    obj3.transform.translation = {0.0f, -0.5f, 0.0f}; 
    obj3.transform.scale = {0.8f, 0.2f, 1.0f}; 
    gameObjects.push_back(std::move(obj3));

    // 2. ★ 카메라 생성 및 세팅 ★
    EngineCamera camera{};
    // 위치는 (0, 0, -2.5)로 살짝 뒤로 물러나서, (0, 0, 0) 원점을 바라보게 세팅!
    camera.setViewTarget(glm::vec3(0.f, 0.f, -2.5f), glm::vec3(0.f, 0.f, 0.f));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.getDevice(), &allocInfo, &commandBuffer);

    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (!window.shouldClose()) {
        window.pollEvents();

        // (동기화 대기/확보)
        VkFence inFlightFence = swapChain.getInFlightFence();
        vkWaitForFences(device.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device.getDevice(), 1, &inFlightFence);

        uint32_t imageIndex;
        VkSemaphore imageAvailable = swapChain.getImageAvailableSemaphore();
        vkAcquireNextImageKHR(device.getDevice(), swapChain.getSwapChain(), UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

        // ★ 매 프레임 화면 비율(Aspect Ratio)에 맞춰 원근감 행렬 갱신 ★
        float aspect = swapChain.getWidth() / (float)swapChain.getHeight();
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

        vkResetCommandBuffer(commandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = swapChain.getRenderPass();
        renderPassInfo.framebuffer = swapChain.getFrameBuffer(imageIndex);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {(uint32_t)WIDTH, (uint32_t)HEIGHT};

        VkClearValue clearColor = {{{0.02f, 0.05f, 0.1f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        // X, Y, Z축을 자유롭게 사용하여 회전
        gameObjects[0].transform.rotation.y = time * glm::radians(90.0f); // Y축 기준으로 빙글빙글(앞뒤로 도는 느낌)
        gameObjects[1].transform.rotation.z = -time * glm::radians(45.0f); // Z축 기준
        gameObjects[2].transform.rotation.x = time * glm::radians(45.0f); // X축 기준 (넘어지는 느낌)

        // ★ 투영 행렬과 뷰 행렬을 미리 곱해둠 (P * V) ★
        auto projectionView = camera.getProjection() * camera.getView();

        for (auto& obj : gameObjects) {
            SimplePushConstantData push{};
            // Projection * View * Model (GPU는 오른쪽에서 왼쪽으로 계산되므로 P * V * M 순서로 곱해야 함)
            push.transform = projectionView * obj.transform.mat4();

            vkCmdPushConstants(
                commandBuffer, 
                pipeline.getPipelineLayout(), 
                VK_SHADER_STAGE_VERTEX_BIT, 
                0, 
                sizeof(SimplePushConstantData), 
                &push
            );

            obj.model->bind(commandBuffer);
            obj.model->draw(commandBuffer);
        }
        
        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);

        // (제출 및 Present 기존과 동일)
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailable};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkSemaphore signalSemaphores[] = {swapChain.getRenderFinishedSemaphore()};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, inFlightFence);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapchains[] = {swapChain.getSwapChain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);
    }

    vkDeviceWaitIdle(device.getDevice());
    return 0;
}