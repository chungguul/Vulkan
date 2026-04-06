#include "EngineWindow.hpp"
#include "EngineDevice.hpp"
#include "EngineSwapChain.hpp"
#include "EnginePipeline.hpp"
#include "EngineModel.hpp"
//#include "EngineGameObject.hpp" //replace entt and Components system
#include "EngineCamera.hpp" // 카메라 추가!
#include "KeyboardMovementController.hpp"
#include "EngineBuffer.hpp"
#include "EngineDescriptorManager.hpp"
#include "EngineTexture.hpp"
#include "EngineAnimation.hpp"
#include "EngineAnimator.hpp"
#include "EnginePhysics.hpp"

#include <entt/entt.hpp>
#include "Components.hpp"

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

const int WIDTH = 1920;
const int HEIGHT = 1080;

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
    EngineAnimation walkAnimation{"../models/Walking.fbx", kedamaModel.get()};
    
    // 애니메이터 생성 시 기본 상태를 Idle로 설정
    EngineAnimator animator{&idleAnimation};    std::cout << "애니메이션 세팅 완료!" << std::endl;
    std::cout << "애니메이션 완료..." << std::endl;
    
    //물리 엔진 클래스 생성 및 초기화 
    EnginePhysics physicsEngine;
    physicsEngine.init();
    //바닥 생성 (무한히 추락 방지)
    physicsEngine.createFloor();

    // EnTT 레지스트리 (데이터베이스) 생성
    entt::registry registry;

    // 캐릭터 엔티티 생성 및 부품 장착
    auto koroneEntity = registry.create();
    auto& koroneTransform = registry.emplace<TransformComponent>(koroneEntity);
    koroneTransform.translation = {0.0f, 10.0f, 0.0f};
    koroneTransform.rotation = {0.0f, 0.0f, 0.0f};
    koroneTransform.scale = {0.01f, 0.01f, 0.01f};
    registry.emplace<ModelComponent>(koroneEntity, kedamaModel);

    //rigid body 부착
    //uint32_t koroneBodyID = physicsEngine.createBox(koroneTransform.translation, glm::vec3(0.5f, 0.5f, 0.5f), true);
    //registry.emplace<RigidBodyComponent>(koroneEntity, koroneBodyID);

    //Ragdoll 장착
    uint32_t koroneRagdollID = physicsEngine.createSimpleRagdoll(koroneTransform.translation);
    registry.emplace<RagdollComponent>(koroneEntity, koroneRagdollID);

    // 3. 뷰어(관찰자 카메라) 엔티티 생성
    auto viewerEntity = registry.create();
    auto& viewerTransform = registry.emplace<TransformComponent>(viewerEntity);
    viewerTransform.translation = {0.f, 0.f, 2.5f};
    
    //키보드 조종기 생성
    KeyboardMovementController cameraController{};

    EngineCamera camera{};
    // 위치는 (0, 0, -2.5)로 살짝 뒤로 물러나서, (0, 0, 0) 원점을 바라보게 세팅!
    camera.setViewTarget(glm::vec3(0.f, 0.0f, -5.0f), glm::vec3(0.f, 10.0f, 0.f));

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

        //매 프레임 물리 연산 업데이트
        physicsEngine.update(frameTime);

        //ECS 동기화 시스템 (Physics -> Render Transform)
        //Transform과 RigidBody를 모두 가진 엔티티만 찾음
        auto physicsView = registry.view<TransformComponent, RigidBodyComponent>();
        for (auto entity : physicsView) {
            auto& transform = physicsView.get<TransformComponent>(entity);
            auto& rigidBody = physicsView.get<RigidBodyComponent>(entity);

            // Jolt 물리 엔진이 계산한 '진짜 위치'를 가져와서, 렌더링용 Transform에 덮어씌웁니다!
            transform.translation = physicsEngine.getBodyPosition(rigidBody.bodyID);
        }

        // 래그돌을 가진 엔티티를 찾아서 뼈대 행렬을 업데이트합니다.
        auto ragdollView = registry.view<TransformComponent, RagdollComponent>();
        for (auto entity : ragdollView) {
            auto& transform = ragdollView.get<TransformComponent>(entity);
            auto& ragdoll = ragdollView.get<RagdollComponent>(entity);

            // 우리가 몸통, 머리 딱 2개의 뼈만 만들었으므로 배열 크기는 2
            glm::mat4 boneMatrices[2]; 
            physicsEngine.updateRagdollBones(ragdoll.ragdollID, boneMatrices, 2);

            // 1. 전체 위치(Root Transform)는 몸통(0번 뼈대)의 위치를 따라가게 합니다.
            // 행렬의 4번째 열(3번 인덱스)이 x, y, z 위치값을 가지고 있습니다.
            transform.translation = glm::vec3(boneMatrices[0][3][0], boneMatrices[0][3][1], boneMatrices[0][3][2]);

            // 2. [선택/중요] 애니메이션 시스템(EngineAnimator) 연동
            // 만약 셰이더(UBO)로 뼈대 행렬을 넘겨주는 애니메이션 시스템이 이미 있다면,
            // 이 boneMatrices[0], boneMatrices[1] 값을 최종 렌더링 뼈대 배열에 덮어씌워주면 됩니다!
            // 예: animator.setBoneMatrices(boneMatrices, 2);
            //animator.setBoneMatrices(boneMatrices, 2);
        }

        // 뷰어 엔티티의 Transform 부품을 가져옵니다.
        auto& viewTrans = registry.get<TransformComponent>(viewerEntity);
        
        // 컨트롤러와 카메라에 부품(viewTrans)을 연결합니다.
        cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, viewTrans);
        camera.setViewYXZ(viewTrans.translation, viewTrans.rotation);


        // ★ 3. 애니메이터 업데이트 및 UBO 전송 (기존과 동일)
        animator.playAnimation(&idleAnimation);
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
        
        auto view = registry.view<TransformComponent, ModelComponent>();
        
        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& modelComp = view.get<ModelComponent>(entity);

            SimplePushConstantData push{};
            push.modelMatrix = transform.mat4();

            vkCmdPushConstants(
                commandBuffer, 
                pipeline.getPipelineLayout(), 
                VK_SHADER_STAGE_VERTEX_BIT, 
                0, 
                sizeof(SimplePushConstantData), 
                &push
            );

            modelComp.model->bind(commandBuffer);
            modelComp.model->draw(commandBuffer);
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