#include "EngineWindow.hpp"
#include "EngineDevice.hpp"
#include "EngineSwapChain.hpp"
#include "EnginePipeline.hpp"
#include "EngineModel.hpp"
#include "EngineGameObject.hpp"
#include "EngineCamera.hpp" // 카메라 추가!
#include "KeyboardMovementController.hpp"
#include "EngineBuffer.hpp"
#include "EngineDescriptorManager.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>


// UBO(유니폼 버퍼)용 구조체 새로 생성
struct GlobalUbo {
    glm::mat4 projectionView;
    glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f}; // RGB 1.0(흰색) + 강도 0.1(10%)
    glm::vec3 lightDirection = glm::normalize(glm::vec3(1.0f, -3.0f, -1.0f)); // 하늘에서 비스듬히 떨어지는 빛
    alignas(16) glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f}; // 직사광선의 색상과 강도
};

// 푸시 상수는 이제 Model 변환만 담당합니다.
struct SimplePushConstantData {
    glm::mat4 modelMatrix{1.0f}; 
};

const int WIDTH = 800;
const int HEIGHT = 600;

int main() {
    EngineWindow window{WIDTH, HEIGHT, "Vulkan Engine"};
    EngineDevice device{window};
    EngineSwapChain swapChain{device, WIDTH, HEIGHT};    //스왑체인

    //디스크립터 세팅
    EngineBuffer uboBuffer{
        device, 
        sizeof(GlobalUbo), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    uboBuffer.map(); // 루프를 돌면서 썼다 지웠다 하지 않고, 계속 연결해 둡니다 (Persistent Mapping)

    // ★ 2. 디스크립터 매니저 생성 및 세팅 (두 줄 컷!)
    EngineDescriptorManager descriptorManager{device};
    descriptorManager.allocateGlobalDescriptorSet(uboBuffer.descriptorInfo());

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; 
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    //파이프라인 세팅
    EnginePipeline pipeline{device, "../Test/shaders/vert.spv", "../Test/shaders/frag.spv", swapChain.getRenderPass(), WIDTH, HEIGHT, {descriptorManager.getGlobalSetLayout()}, {pushConstantRange}};
    
    // 피라미드 obj파일 로드
    std::cout << "모델 로딩 중..." << std::endl;
    EngineModel::Builder pyramidBuilder{};
    // 경로는 실행 파일 위치 기준(build 폴더)이므로 부모 폴더의 models를 가리킵니다.
    pyramidBuilder.loadModel("../models/pyramid.obj"); 
    auto pyramidModel = std::make_shared<EngineModel>(device, pyramidBuilder);
    std::cout << "모델 로딩 완료!" << std::endl;

    // 게임 오브젝트들에게 피라미드 모델을 장착시켜 줍니다!
    std::vector<EngineGameObject> gameObjects;

    auto obj1 = EngineGameObject::createGameObject();
    obj1.model = pyramidModel; // 큐브 대신 피라미드 장착
    obj1.transform.translation = {-0.5f, 0.0f, 0.0f}; 
    obj1.transform.scale = {0.5f, 0.5f, 0.5f}; 
    gameObjects.push_back(std::move(obj1));

    auto obj2 = EngineGameObject::createGameObject();
    obj2.model = pyramidModel;
    obj2.transform.translation = {0.5f, 0.0f, 0.0f}; 
    obj2.transform.scale = {0.8f, 0.8f, 0.8f}; 
    gameObjects.push_back(std::move(obj2));

    auto obj3 = EngineGameObject::createGameObject();
    obj3.model = pyramidModel;
    obj3.transform.translation = {0.0f, -0.5f, 0.0f}; 
    obj3.transform.scale = {0.8f, 0.2f, 1.0f}; 
    gameObjects.push_back(std::move(obj3));

    // 플레이어의 눈이 되어줄 "뷰어(Viewer)" 게임 오브젝트를 만듭니다.
    EngineGameObject viewerObject = EngineGameObject::createGameObject();
    // 시작 위치를 살짝 뒤로 물러나게 설정
    viewerObject.transform.translation = {0.f, 0.f, 2.5f};
    
    //키보드 조종기 생성
    KeyboardMovementController cameraController{};

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

    std::cout << "엔진 루프 진입 중..." << std::endl;
    auto currentTime = std::chrono::high_resolution_clock::now();
    
    while (!window.shouldClose()) {
        window.pollEvents();

        //Delta Time (dt) 계산
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        // ★ 키보드 입력을 받아 뷰어 오브젝트를 움직입니다!
        cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, viewerObject);
        
        // ★ 카메라의 뷰 행렬을 뷰어 오브젝트의 위치/회전 값으로 덮어씌웁니다.
        camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        // ★ 매 프레임 화면 비율(Aspect Ratio)에 맞춰 원근감 행렬 갱신 ★
        float aspect = swapChain.getWidth() / (float)swapChain.getHeight();
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

        // (동기화 대기/확보)
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
        
        VkClearValue clearDepth{};
        clearDepth.depthStencil = {1.0f, 0};

        std::vector<VkClearValue> clearValues = {clearColor, clearDepth};
        // ==========================================================

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
        
        //auto currentTime = std::chrono::high_resolution_clock::now();
        //float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        // X, Y, Z축을 자유롭게 사용하여 회전 delta time으로 변경
        gameObjects[0].transform.rotation.y += frameTime * glm::radians(90.0f); // Y축 기준으로 빙글빙글(앞뒤로 도는 느낌)
        gameObjects[1].transform.rotation.z -= frameTime * glm::radians(45.0f); // Z축 기준
        gameObjects[2].transform.rotation.x += frameTime * glm::radians(45.0f); // X축 기준 (넘어지는 느낌)

        // ★ 투영 행렬과 뷰 행렬을 미리 곱해둠 (P * V) ★
        auto projectionView = camera.getProjection() * camera.getView();

        //오브젝트를 그리기 전 1회 UBO데이터 업데이트
        GlobalUbo ubo{};
        ubo.projectionView = camera.getProjection() * camera.getView();
        uboBuffer.writeToBuffer(&ubo);

        //파이프라인에 디스크립터 셋(통신망) 장착!
        VkDescriptorSet globalSet = descriptorManager.getGlobalDescriptorSet();
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipelineLayout(), 0, 1, &globalSet, 0, nullptr);
        
        for (auto& obj : gameObjects) {
            SimplePushConstantData push{};
            // Projection * View * Model (GPU는 오른쪽에서 왼쪽으로 계산되므로 P * V * M 순서로 곱해야 함)
            //push.transform = projectionView * obj.transform.mat4();
            push.modelMatrix = obj.transform.mat4();

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
    std::cout << "엔진 정상 종료 및 메모리 정리 완료." << std::endl;
    return 0;
}