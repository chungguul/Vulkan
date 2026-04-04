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
#include "EngineTexture.hpp"
#include "EngineAnimation.hpp"
#include "EngineAnimator.hpp"

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>

const int MAX_BONES = 100;

// UBO(유니폼 버퍼)용 구조체 새로 생성
struct GlobalUbo {
    glm::mat4 projectionView;
    glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f}; // RGB 1.0(흰색) + 강도 0.1(10%)
    glm::vec3 lightDirection = glm::normalize(glm::vec3(1.0f, -3.0f, -1.0f)); // 하늘에서 비스듬히 떨어지는 빛
    alignas(16) glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f}; // 직사광선의 색상과 강도

    glm::mat4 finalBonesMatrices[MAX_BONES];
};

// 푸시 상수는 이제 Model 변환만 담당합니다.
struct SimplePushConstantData {
    glm::mat4 modelMatrix{1.0f}; 
};

const int WIDTH = 1280;
const int HEIGHT = 800;

int main() {
    EngineWindow window{WIDTH, HEIGHT, "Vulkan Engine"};
    EngineDevice device{window};
    EngineSwapChain swapChain{device, WIDTH, HEIGHT};    //스왑체인

    //UBO 세팅
    EngineBuffer uboBuffer{
        device, 
        sizeof(GlobalUbo), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    uboBuffer.map(); // 루프를 돌면서 썼다 지웠다 하지 않고, 계속 연결해 둡니다 (Persistent Mapping)

    //텍스처 세팅
    std::cout << "텍스처 로딩 중..." << std::endl;
    EngineTexture myTexture{device, "../textures/Korone_Map.png"};
    
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = myTexture.getImageView();
    imageInfo.sampler = myTexture.getSampler();
    std::cout << "텍스처 로딩 완료..." << std::endl;
    // ★ 2. 디스크립터 매니저 생성 및 세팅 (두 줄 컷!)
    EngineDescriptorManager descriptorManager{device};
    descriptorManager.allocateGlobalDescriptorSet(uboBuffer.descriptorInfo(),imageInfo);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; 
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    //파이프라인 세팅
    EnginePipeline pipeline{device, "../Test/shaders/vert.spv", "../Test/shaders/frag.spv", swapChain.getRenderPass(), WIDTH, HEIGHT, {descriptorManager.getGlobalSetLayout()}, {pushConstantRange}};
    
    // .fbx 모델 추출
    std::cout << "모델 로딩 중..." << std::endl;
    EngineModel::Builder kedamaBuilder{};
    // 경로는 실행 파일 위치 기준(build 폴더)이므로 부모 폴더의 models를 가리킵니다.
    kedamaBuilder.loadModel("../models/KedamaKorone.fbx"); 
    auto kedamaModel = std::make_shared<EngineModel>(device, kedamaBuilder);
    std::cout << "모델 로딩 완료!" << std::endl;

    // 애니메이션 로딩 및 애니메이터 생성
    std::cout << "애니메이션 로딩 중..." << std::endl;
    // 1. 숨쉬기(Idle) 애니메이션 (기존 원본 FBX 사용)
    EngineAnimation idleAnimation{"../models/KedamaKorone.fbx", kedamaModel.get()};
    
    // 2. 걷기(Walk) 애니메이션 (새로 받은 뼈대 전용 FBX 사용)
    //EngineAnimation walkAnimation{"../models/Korone_Walk.fbx", kedamaModel.get()};
    
    // 애니메이터 생성 시 기본 상태를 Idle로 설정
    EngineAnimator animator{&idleAnimation};    std::cout << "애니메이션 세팅 완료!" << std::endl;

    // 게임 오브젝트들에게 캐릭터 모델을 장착시켜 줍니다!
    std::vector<EngineGameObject> gameObjects;

    auto obj1 = EngineGameObject::createGameObject();
    obj1.model = kedamaModel; // 캐릭터 장착
    obj1.transform.translation = {0.0f, 0.0f, 0.0f};
    //obj1.transform.rotation = {-glm::radians(90.0f), 0.0f, 0.0f};
    obj1.transform.scale = {0.03f, 0.03f, 0.03f}; 
    gameObjects.push_back(std::move(obj1));

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
    
    static bool wasMoving = false;
    while (!window.shouldClose()) {
        window.pollEvents();

        //Delta Time (dt) 계산
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        // ★ 키보드 입력을 받아 뷰어 오브젝트를 움직입니다!
        bool isMoving = cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, gameObjects[0]);
        
        // ★ 2. 애니메이션 상태 전이 (State Machine)
        if (isMoving && !wasMoving) {
            // 멈춰있다가 방금 걷기 시작함 -> Walk 애니메이션 재생
            //animator.playAnimation(&walkAnimation);
            wasMoving = true;
        } else if (!isMoving && wasMoving) {
            // 걷다가 방금 멈춤 -> Idle 애니메이션 재생
            animator.playAnimation(&idleAnimation);
            wasMoving = false;
        }

        // 카메라 세팅 (카메라는 고정된 뷰어 오브젝트의 위치를 그대로 씁니다)
        camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        // ★ 3. 애니메이터 업데이트 및 UBO 전송 (기존과 동일)
        animator.updateAnimation(frameTime);
        
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
        //gameObjects[0].transform.rotation.y += frameTime * glm::radians(90.0f); // Y축 기준으로 빙글빙글(앞뒤로 도는 느낌)
        //gameObjects[1].transform.rotation.z -= frameTime * glm::radians(45.0f); // Z축 기준
        //gameObjects[2].transform.rotation.x += frameTime * glm::radians(45.0f); // X축 기준 (넘어지는 느낌)

        // ★ 투영 행렬과 뷰 행렬을 미리 곱해둠 (P * V) ★
        auto projectionView = camera.getProjection() * camera.getView();

        //애니메이터 업데이트 delta time만큼 애니메이션 진행
        animator.updateAnimation(frameTime);

        //오브젝트를 그리기 전 1회 UBO데이터 업데이트
        GlobalUbo ubo{};
        ubo.projectionView = camera.getProjection() * camera.getView();
        
        //애니메이터가 계산한 최신 뼈대 행렬 100개를 UBO 구조체로 복사
        auto transforms = animator.getFinalBoneMatrices();
        for (int i = 0; i < MAX_BONES; ++i) {
            ubo.finalBonesMatrices[i] = transforms[i];
        }
        
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