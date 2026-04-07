#include "EngineWindow.hpp"
#include "EngineDevice.hpp"
#include "EngineSwapChain.hpp"
#include "EnginePipeline.hpp"
#include "EngineModel.hpp"
// #include "EngineGameObject.hpp" //replace entt and Components system
#include "EngineCamera.hpp" // 카메라 추가!
#include "KeyboardMovementController.hpp"
#include "EngineBuffer.hpp"
#include "EngineDescriptorManager.hpp"
#include "EngineTexture.hpp"
#include "EngineAnimation.hpp"
#include "EngineAnimator.hpp"
#include "EnginePhysics.hpp"
#include "EngineCubemap.hpp"
#include "EngineSkybox.hpp"
#include "EngineShadow.hpp"

#include <glm/gtx/matrix_decompose.hpp>

#include <entt/entt.hpp>
#include "Components.hpp"

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>

const int MAX_BONES = 100;

// UBO(유니폼 버퍼)용 구조체 새로 생성
struct GlobalUbo
{
    glm::mat4 projectionView;
    glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f};                      // RGB 1.0(흰색) + 강도 0.1(10%)
    glm::vec3 lightDirection = glm::normalize(glm::vec3(1.0f, -3.0f, -1.0f)); // 하늘에서 비스듬히 떨어지는 빛
    alignas(16) glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};                 // 직사광선의 색상과 강도

    glm::mat4 finalBonesMatrices[MAX_BONES];

    glm::mat4 view;
    glm::mat4 proj;

    glm::mat4 lightSpaceMatrix;
};

// 푸시 상수는 이제 Model 변환만 담당합니다.
struct SimplePushConstantData
{
    glm::mat4 modelMatrix{1.0f};
};

const int WIDTH = 1920;
const int HEIGHT = 1080;

int main()
{
    try
    {
        EngineWindow window{WIDTH, HEIGHT, "Vulkan Engine"};
        EngineDevice device{window};
        EngineSwapChain swapChain{device, WIDTH, HEIGHT}; // 스왑체인

        // UBO 세팅
        EngineBuffer uboBuffer{
            device,
            sizeof(GlobalUbo),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        uboBuffer.map(); // 루프를 돌면서 썼다 지웠다 하지 않고, 계속 연결해 둡니다 (Persistent Mapping)

        // 텍스처 세팅
        std::cout << "텍스처 로딩 중..." << std::endl;
        EngineTexture myTexture{device, "../textures/Korone_Map.png"};
        EngineTexture woodTexture{device, "../textures/wood1.jpg"};

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = myTexture.getImageView();
        imageInfo.sampler = myTexture.getSampler();

        // 바닥용 평면(Plane) 모델 수동 생성
        EngineModel::Builder floorBuilder{};
        floorBuilder.vertices = {
            {{-20.0f, 0.0f, -20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-20.0f, 0.0f,  20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 10.0f}},
            {{ 20.0f, 0.0f,  20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {10.0f, 10.0f}},
            {{ 20.0f, 0.0f, -20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {10.0f, 0.0f}}
        };
        floorBuilder.indices = {0, 1, 2, 2, 3, 0};
        auto floorModel = std::make_shared<EngineModel>(device, floorBuilder);

        // HDR 스카이박스 텍스쳐 로딩
        EngineTexture hdrSkyboxTexture{device};
        hdrSkyboxTexture.loadHDR("../textures/sunflowers_puresky_4k.hdr");
        EngineCubemap skyboxCubemap{device, hdrSkyboxTexture, 4096};

        std::cout << "텍스처 로딩 완료..." << std::endl;
        // ★ 2. 디스크립터 매니저 생성 및 세팅 (두 줄 컷!)
        EngineDescriptorManager descriptorManager{device};

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SimplePushConstantData);

        // 파이프라인 세팅
        PipelineConfigInfo pipelineConfig{};
        EnginePipeline::defaultPipelineConfigInfo(pipelineConfig, WIDTH, HEIGHT);

        pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        //pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        pipelineConfig.renderPass = swapChain.getRenderPass();
        pipelineConfig.descriptorSetLayouts = {descriptorManager.getGlobalSetLayout()};
        pipelineConfig.pushConstantRanges = {pushConstantRange};

        EnginePipeline pipeline{
            device,
            "../Test/shaders/vert.spv",
            "../Test/shaders/frag.spv",
            pipelineConfig};

        // .fbx 모델 추출
        std::cout << "모델 로딩 중..." << std::endl;
        EngineModel::Builder kedamaBuilder{};
        // 경로는 실행 파일 위치 기준(build 폴더)이므로 부모 폴더의 models를 가리킵니다.
        kedamaBuilder.loadModel("../models/Kedama_scaled_v3.fbx");
        auto kedamaModel = std::make_shared<EngineModel>(device, kedamaBuilder);
        std::cout << "모델 로딩 완료!" << std::endl;

        // 애니메이션 로딩 및 애니메이터 생성
        std::cout << "애니메이션 로딩 중..." << std::endl;
        // 1. 숨쉬기(Idle) 애니메이션 (기존 원본 FBX 사용)
        EngineAnimation idleAnimation{"../models/KedamaKorone.fbx", kedamaModel.get()};

        // 2. 걷기(Walk) 애니메이션 (새로 받은 뼈대 전용 FBX 사용)
        EngineAnimation walkAnimation{"../models/Walking.fbx", kedamaModel.get()};

        // 애니메이터 생성 시 기본 상태를 Idle로 설정
        EngineAnimator animator{&idleAnimation};
        std::cout << "애니메이션 세팅 완료!" << std::endl;
        std::cout << "애니메이션 완료..." << std::endl;

        // 물리 엔진 클래스 생성 및 초기화
        EnginePhysics physicsEngine;
        physicsEngine.init();
        // 바닥 생성 (무한히 추락 방지)
        physicsEngine.createFloor();

        // EnTT 레지스트리 (데이터베이스) 생성
        entt::registry registry;

        // 캐릭터 엔티티 생성 및 부품 장착
        auto koroneEntity = registry.create();
        auto &koroneTransform = registry.emplace<TransformComponent>(koroneEntity);
        koroneTransform.translation = {0.0f, 10.0f, 0.0f};
        koroneTransform.rotation = {0.0f, 0.0f, 0.0f};
        koroneTransform.scale = {0.01f, 0.01f, 0.01f};
        // koroneTransform.scale = {0.1f,0.1f, 0.1f};
        registry.emplace<ModelComponent>(koroneEntity, kedamaModel);

        // 바닥 엔티티 생성
        auto floorEntity = registry.create();
        auto &floorTransform = registry.emplace<TransformComponent>(floorEntity);
        floorTransform.translation = {0.0f, 0.0f, 0.0f}; 
        floorTransform.scale = {1.0f, 1.0f, 1.0f};
        registry.emplace<ModelComponent>(floorEntity, floorModel);

        // rigid body 부착
        //uint32_t koroneBodyID = physicsEngine.createBox(koroneTransform.translation, glm::vec3(0.5f, 0.5f, 0.5f), true);
        //registry.emplace<RigidBodyComponent>(koroneEntity, koroneBodyID);

        // Ragdoll 장착
        uint32_t koroneRagdollID = physicsEngine.createSimpleRagdoll(koroneTransform.translation);
        registry.emplace<RagdollComponent>(koroneEntity, koroneRagdollID);

        // 3. 뷰어(관찰자 카메라) 엔티티 생성
        auto viewerEntity = registry.create();
        auto &viewerTransform = registry.emplace<TransformComponent>(viewerEntity);
        viewerTransform.translation = {0.f, 5.0f, -5.0f};

        // 키보드 조종기 생성
        KeyboardMovementController cameraController{};

        EngineCamera camera{};
        // 위치는 (0, 0, -2.5)로 살짝 뒤로 물러나서, (0, 0, 0) 원점을 바라보게 세팅!
        camera.setViewTarget(glm::vec3(0.f, 10.0f, -5.0f), glm::vec3(0.f, 0.0f, 0.f));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = device.getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device.getDevice(), &allocInfo, &commandBuffer);

        std::vector<VkBuffer> uboBufferArray = {uboBuffer.getBuffer()};

        EngineSkybox skyboxRenderer{
            device,
            swapChain.getRenderPass(),
            WIDTH,
            HEIGHT,
            skyboxCubemap,
            descriptorManager.getGlobalSetLayout(),
            uboBufferArray,
            sizeof(GlobalUbo)
        };

        std::cout << "그림자 시스템 로딩 중..." << std::endl;
        
        // 1. 그림자 도화지(Framebuffer) 객체 생성 (2048x2048 해상도)
        EngineShadow engineShadow{device, 2048, 2048};

        // 1. 코로네 텍스처용 정보
        VkDescriptorImageInfo koroneImageInfo{};
        koroneImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        koroneImageInfo.imageView = myTexture.getImageView();
        koroneImageInfo.sampler = myTexture.getSampler();

        // 2. 바닥 나무 텍스처용 정보
        VkDescriptorImageInfo floorImageInfo{};
        floorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        floorImageInfo.imageView = woodTexture.getImageView();
        floorImageInfo.sampler = woodTexture.getSampler();

        // 3. 섀도우 맵 텍스처 정보
        VkDescriptorImageInfo shadowImageInfo{};
        shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowImageInfo.imageView = engineShadow.getImageView();
        shadowImageInfo.sampler = engineShadow.getSampler();

        // ★ 대망의 세트 분리! 이제 공장에서 2개의 세트를 독립적으로 찍어냅니다.
        VkDescriptorSet koroneSet = descriptorManager.allocateDescriptorSet(uboBuffer.descriptorInfo(), koroneImageInfo, shadowImageInfo);
        VkDescriptorSet floorSet = descriptorManager.allocateDescriptorSet(uboBuffer.descriptorInfo(), floorImageInfo, shadowImageInfo);

        // 2. 그림자 전용 파이프라인 설정
        PipelineConfigInfo shadowPipelineConfig{};
        // 해상도는 스왑체인(화면) 크기가 아니라 그림자 도화지의 크기를 따라갑니다!
        EnginePipeline::defaultPipelineConfigInfo(shadowPipelineConfig, engineShadow.getWidth(), engineShadow.getHeight());
        
        // 컬러 블렌딩 설정을 반드시 0으로 꺼주어야 Vulkan이 뻗지 않습니다!
        shadowPipelineConfig.colorBlendInfo.attachmentCount = 0;
        shadowPipelineConfig.colorBlendInfo.pAttachments = nullptr;

        // 피터팬 현상(그림자가 공중에 뜨는 현상) 방지를 위해 앞면을 잘라냅니다.
        shadowPipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
        // 메인 화면이 아닌 그림자 도화지의 렌더패스를 연결!
        shadowPipelineConfig.renderPass = engineShadow.getRenderPass(); 
        
        shadowPipelineConfig.descriptorSetLayouts = {descriptorManager.getGlobalSetLayout()};
        shadowPipelineConfig.pushConstantRanges = {pushConstantRange};

        // 3. 그림자 파이프라인 생성
        EnginePipeline shadowPipeline{
            device,
            "../Test/shaders/shadow.vert.spv",
            "../Test/shaders/shadow.frag.spv",
            shadowPipelineConfig
        };

        std::cout << "그림자 시스템 로딩 완료..." << std::endl;


        std::cout << "엔진 루프 진입 중..." << std::endl;
        auto currentTime = std::chrono::high_resolution_clock::now();

        static bool wasMoving = false;
        while (!window.shouldClose())
        {
            window.pollEvents();

            // 1. Delta Time (dt) 계산
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            // 2. 물리 연산 업데이트
            physicsEngine.update(frameTime);

            // ★ UBO 객체를 여기서 '선언'만 합니다. (아직 GPU로 전송 안 함!)
            GlobalUbo ubo{};

            // 3. ECS 물리 동기화
            auto physicsView = registry.view<TransformComponent, RigidBodyComponent>();
            for (auto entity : physicsView) {
                auto &transform = physicsView.get<TransformComponent>(entity);
                auto &rigidBody = physicsView.get<RigidBodyComponent>(entity);
                transform.translation = physicsEngine.getBodyPosition(rigidBody.bodyID);
            }

            // 4. 래그돌 뼈대 행렬 계산 (계산된 값을 ubo에 넣습니다!)
            auto ragdollView = registry.view<TransformComponent, RagdollComponent, ModelComponent>();
            for (auto entity : ragdollView) {
                auto &transform = ragdollView.get<TransformComponent>(entity);
                auto &ragdoll = ragdollView.get<RagdollComponent>(entity);
                auto &modelComp = ragdollView.get<ModelComponent>(entity);

                physicsEngine.syncRagdollBones(ragdoll.ragdollID, modelComp.model->getBoneInfoMap(), ubo.finalBonesMatrices, transform.translation, transform.rotation);

                // 임펄스 처리 (스페이스바)
                static bool spacePressed = false;
                if (glfwGetKey(window.getGLFWwindow(), GLFW_KEY_SPACE) == GLFW_PRESS) {
                    if (!spacePressed) {
                        physicsEngine.applyImpulseToRagdoll(ragdoll.ragdollID, glm::vec3(0.0f, 300.0f, -500.0f), 0);
                        spacePressed = true;
                    }
                } else {
                    spacePressed = false;
                }
            }

            // 5. 카메라 및 조종기 업데이트
            auto &koroneTrans = registry.get<TransformComponent>(koroneEntity);
            auto &viewTrans = registry.get<TransformComponent>(viewerEntity);
            cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, viewTrans);
            camera.setViewTarget(viewTrans.translation, koroneTrans.translation);

            // 화면 비율에 맞춰 원근감(Projection) 행렬을 계산합니다!
            float aspect = swapChain.getWidth() / (float)swapChain.getHeight();
            camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 1000.f);

            // 애니메이터 업데이트
            animator.playAnimation(&idleAnimation);
            animator.updateAnimation(frameTime);

            // ==========================================================
            // ★ 6. 모든 계산이 완료된 '진짜 완성본 데이터'를 UBO에 채웁니다.
            // ==========================================================
            ubo.view = camera.getView();
            ubo.proj = camera.getProjection();

            ubo.proj[1][1] *= -1.0f;

            ubo.projectionView = ubo.proj * ubo.view;

            // 그림자 전용 빛의 시야 계산
            glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 50.0f);
            lightProjection[1][1] *= -1.0f; 
            glm::vec3 lightPos = -ubo.lightDirection * 20.0f; 
            glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            ubo.lightSpaceMatrix = lightProjection * lightView;

            // ★ 7. 꽉 채워진 UBO를 비로소 GPU로 전송합니다! (버스 출발)
            uboBuffer.writeToBuffer(&ubo);
            // ==========================================================

            // (동기화 대기/확보)
            VkFence inFlightFence = swapChain.getInFlightFence();
            vkWaitForFences(device.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device.getDevice(), 1, &inFlightFence);

            uint32_t imageIndex;
            VkSemaphore imageAvailable = swapChain.getImageAvailableSemaphore();
            vkAcquireNextImageKHR(device.getDevice(), swapChain.getSwapChain(), UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

            // ★ 중복 에러 해결: 커맨드 버퍼 초기화 및 선언은 여기서 한 번만!
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // ==========================================================
            // 패스 1: 섀도우 패스 (빛의 시점에서 깊이맵 굽기)
            // ==========================================================
            VkRenderPassBeginInfo shadowPassInfo{};
            shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            shadowPassInfo.renderPass = engineShadow.getRenderPass();
            shadowPassInfo.framebuffer = engineShadow.getFramebuffer();
            shadowPassInfo.renderArea.offset = {0, 0};
            shadowPassInfo.renderArea.extent = {engineShadow.getWidth(), engineShadow.getHeight()};

            VkClearValue depthClear{};
            depthClear.depthStencil = {1.0f, 0}; // 깊이 초기화
            shadowPassInfo.clearValueCount = 1;
            shadowPassInfo.pClearValues = &depthClear;

            vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.getPipeline());

            auto view = registry.view<TransformComponent, ModelComponent>();
            for (auto entity : view) {
                auto &transform = view.get<TransformComponent>(entity);
                auto &modelComp = view.get<ModelComponent>(entity);

                SimplePushConstantData push{};
                push.modelMatrix = transform.mat4();
                vkCmdPushConstants(commandBuffer, shadowPipeline.getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SimplePushConstantData), &push);

                VkDescriptorSet currentSet = (modelComp.model == kedamaModel) ? koroneSet : floorSet;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.getPipelineLayout(), 0, 1, &currentSet, 0, nullptr);

                modelComp.model->bind(commandBuffer);
                modelComp.model->draw(commandBuffer);
            }
            vkCmdEndRenderPass(commandBuffer);

            // ==========================================================
            // 패스 2: 메인 렌더 패스 (진짜 화면에 그리기)
            // ==========================================================
            // ★ 중복 에러 해결: renderPassInfo 선언도 여기서 한 번만!
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
            
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();
            
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());

            for (auto entity : view) {
                auto &transform = view.get<TransformComponent>(entity);
                auto &modelComp = view.get<ModelComponent>(entity);

                SimplePushConstantData push{};
                push.modelMatrix = transform.mat4();
                vkCmdPushConstants(commandBuffer, pipeline.getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SimplePushConstantData), &push);

                // ★ 모델이 누구냐에 따라 디스크립터 셋(텍스처)을 갈아 끼웁니다!
                VkDescriptorSet currentSet = (modelComp.model == kedamaModel) ? koroneSet : floorSet;
                
                vkCmdBindDescriptorSets(
                    commandBuffer, 
                    VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    pipeline.getPipelineLayout(), 
                    0, 1, &currentSet, 0, nullptr
                );

                modelComp.model->bind(commandBuffer);
                modelComp.model->draw(commandBuffer);
            }

            skyboxRenderer.render(commandBuffer, 0);

            vkCmdEndRenderPass(commandBuffer);
            vkEndCommandBuffer(commandBuffer);
            // ==========================================================

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
    catch (const std::exception &e)
    {
        // 에러가 나면 abort() 대신 콘솔에 정확한 이유를 출력합니다!
        std::cerr << "\n[치명적 에러 발생] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}