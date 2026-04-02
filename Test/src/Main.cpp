#include "EngineWindow.hpp"
#include "EngineDevice.hpp"
#include "EngineSwapChain.hpp"
#include "EnginePipeline.hpp"
#include "EngineModel.hpp"
#include "EngineGameObject.hpp" // 새로 추가!
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>

struct SimplePushConstantData {
    glm::mat2 transform{1.0f};
    glm::vec2 offset;
    alignas(16) glm::vec3 color;
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

    // 1. 공용으로 사용할 모델(사각형 데이터)을 메모리에 딱 1번만 올립니다. (shared_ptr 사용)
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
    };
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    auto quadModel = std::make_shared<EngineModel>(device, vertices, indices);

    // 2. 게임 오브젝트 리스트 생성
    std::vector<EngineGameObject> gameObjects;

    // 첫 번째 오브젝트 (왼쪽에 작고 빠르게 도는 사각형)
    auto obj1 = EngineGameObject::createGameObject();
    obj1.model = quadModel; // 똑같은 모델 공유
    obj1.transform2d.translation = {-0.5f, 0.0f}; // 왼쪽으로 0.5 이동
    obj1.transform2d.scale = {0.5f, 0.5f}; // 크기 반으로 줄임
    gameObjects.push_back(std::move(obj1));

    // 두 번째 오브젝트 (오른쪽에 크고 천천히 반대로 도는 사각형)
    auto obj2 = EngineGameObject::createGameObject();
    obj2.model = quadModel;
    obj2.transform2d.translation = {0.5f, 0.0f}; // 오른쪽으로 0.5 이동
    obj2.transform2d.scale = {0.8f, 0.8f}; // 크기 0.8배
    gameObjects.push_back(std::move(obj2));

    // 세 번째 오브젝트 (위에서 가만히 찌그러져 있는 사각형)
    auto obj3 = EngineGameObject::createGameObject();
    obj3.model = quadModel;
    obj3.transform2d.translation = {0.0f, -0.5f}; // 위쪽으로 0.5 이동
    obj3.transform2d.scale = {0.8f, 0.2f}; // 넓적하게 찌그러트림
    gameObjects.push_back(std::move(obj3));


    // ... 커맨드 버퍼 할당 코드 (기존과 동일) ...
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.getDevice(), &allocInfo, &commandBuffer);

    std::cout << "엔진 루프 진입 중..." << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (!window.shouldClose()) {
        window.pollEvents();

        // (동기화 대기 및 이미지 확보 등 기존 코드 동일)
        VkFence inFlightFence = swapChain.getInFlightFence();
        vkWaitForFences(device.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device.getDevice(), 1, &inFlightFence);

        uint32_t imageIndex;
        VkSemaphore imageAvailable = swapChain.getImageAvailableSemaphore();
        vkAcquireNextImageKHR(device.getDevice(), swapChain.getSwapChain(), UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

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
        
        // 시간 계산
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        // ==========================================================
        // ★ 오브젝트 리스트를 순회하며 렌더링 ★
        // 여기서 각 오브젝트별로 독립적인 위치/회전/크기 연산을 수행합니다!
        
        // obj1은 시계 방향으로 빠르게 회전
        gameObjects[0].transform2d.rotation = time * glm::radians(180.0f);
        // obj2는 반시계 방향으로 천천히 회전
        gameObjects[1].transform2d.rotation = -time * glm::radians(45.0f);
        // obj3은 회전하지 않음 (초기값 0.0f 유지)

        for (auto& obj : gameObjects) {
            SimplePushConstantData push{};
            push.offset = obj.transform2d.translation; // 구조체에서 위치 가져오기
            push.transform = obj.transform2d.mat2();   // 크기+회전 행렬 계산 가져오기

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
        // ==========================================================
        
        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);

        // (큐 제출 및 화면 출력 등 기존 코드 동일)
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
    std::cout << "엔진 정상 종료됨." << std::endl;
    return 0;
}