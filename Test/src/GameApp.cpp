#include "GameApp.hpp"
#include <iostream>
#include <chrono>
#include <fstream>
#include <cmath>


using json = nlohmann::json;

// ==========================================================
// 1. 엔진 초기화 (순서가 매우 중요합니다!)
// ==========================================================
GameApp::GameApp() {
    std::cout << "엔진 코어 초기화 중..." << std::endl;
    
    assetManager = std::make_unique<AssetManager>(device); 
    threadPool = std::make_unique<EngineThreadPool>();
    engineRenderer = std::make_unique<EngineRenderer>(window, device);

    // 1-1. 물리 엔진 초기화
    physicsEngine.init();
    physicsEngine.createFloor();

    // 1-3. UBO 버퍼 생성 및 매핑
    uboBufferMain = std::make_unique<EngineBuffer>(device, sizeof(GlobalUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uboBufferMain->map();
    uboBufferReflection = std::make_unique<EngineBuffer>(device, sizeof(GlobalUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uboBufferReflection->map();
    uboBufferRefraction = std::make_unique<EngineBuffer>(device, sizeof(GlobalUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uboBufferRefraction->map();

    // 1-4. 텍스처 및 스카이박스 로딩
    std::cout << "코어 에셋 로딩 중..." << std::endl;

    engineWater = std::make_unique<EngineWater>(device, WIDTH, HEIGHT);
    engineShadow = std::make_unique<EngineShadow>(device, 2048, 2048);
    descriptorManager = std::make_unique<EngineDescriptorManager>(device);
}

GameApp::~GameApp() {
    vkDeviceWaitIdle(device.getDevice());
    std::cout << "엔진 정상 종료 및 메모리 정리 완료." << std::endl;
}
// ==========================================================
// 3. 디스크립터 조립 & 파이프라인 생성
// ==========================================================
void GameApp::setupDescriptorsAndPipelines() {
    std::cout << "파이프라인 및 디스크립터 세팅 중..." << std::endl;

    // 레이아웃 생성
    std::vector<VkDescriptorSetLayoutBinding> globalBindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    globalSetLayout = descriptorManager->createDescriptorSetLayout(globalBindings);

    std::vector<VkDescriptorSetLayoutBinding> waterBindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, 
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, 
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, 
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}  
    };
    waterSetLayout = descriptorManager->createDescriptorSetLayout(waterBindings);

    // 이미지 정보 묶기
    auto makeImgInfo = [](VkImageLayout layout, VkImageView view, VkSampler sampler) {
        VkDescriptorImageInfo info{}; info.imageLayout = layout; info.imageView = view; info.sampler = sampler; return info;
    };    
    auto shadowImageInfo = makeImgInfo(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, engineShadow->getImageView(), engineShadow->getSampler());
    auto skyboxInfo      = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, skyboxCubemap->getImageView(), skyboxCubemap->getSampler());
    auto reflectionInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, engineWater->getReflectionImageView(), engineWater->getSampler());
    auto refractionInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, engineWater->getRefractionImageView(), engineWater->getSampler());
        
    // 버퍼 정보
    auto uboInfoMain = uboBufferMain->descriptorInfo();
    auto uboInfoReflection = uboBufferReflection->descriptorInfo();
    auto uboInfoRefraction = uboBufferRefraction->descriptorInfo();

    // 디스크립터 세트 조립
    auto renderableView = registry.view<ModelComponent, MaterialComponent>();
    for (auto entity : renderableView) {
        auto &modelComp = renderableView.get<ModelComponent>(entity);
        auto &matComp = renderableView.get<MaterialComponent>(entity);

        // 1. 컴포넌트에 적힌 이름표를 보고 AssetManager에서 텍스처를 꺼내옵니다.
        auto albedoTex = assetManager->getTexture(matComp.albedoTexture);
        auto albedoInfo = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, albedoTex->getImageView(), albedoTex->getSampler());

        // 2. Main 디스크립터 조립
        EngineDescriptorManager::Builder(*descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
            .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &albedoInfo) // ★ 동적 할당!
            .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
            .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &skyboxInfo)
            .build(modelComp.mainSet);

        // 3. 반사(Reflection) 디스크립터 조립
        EngineDescriptorManager::Builder(*descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoReflection)
            .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &albedoInfo)
            .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
            .build(modelComp.reflectionSet);

        // 4. 굴절(Refraction) 디스크립터 조립
        EngineDescriptorManager::Builder(*descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoRefraction)
            .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &albedoInfo)
            .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
            .build(modelComp.refractionSet);
    }

    auto waterView = registry.view<WaterComponent>();
    for (auto entity : waterView) {
        auto& water = waterView.get<WaterComponent>(entity);
        
        auto dudvTex   = assetManager->getTexture(water.dudvTexture);
        auto normalTex = assetManager->getTexture(water.normalTexture);

        auto dudvInfo   = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, dudvTex->getImageView(), dudvTex->getSampler());
        auto normalInfo = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, normalTex->getImageView(), normalTex->getSampler());

        EngineDescriptorManager::Builder(*descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
            .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &reflectionInfo)
            .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &refractionInfo)
            .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &dudvInfo)   
            .bindImage(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &normalInfo) 
            .build(water.waterSet);
    }


    // 파이프라인 생성!
    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(device, engineRenderer->getSwapChainRenderPass(), globalSetLayout);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    shadowSystem = std::make_unique<EngineShadowSystem>(device, engineShadow->getRenderPass(), globalSetLayout);
    waterRenderSystem = std::make_unique<EngineWaterSystem>(device, engineRenderer->getSwapChainRenderPass(), waterSetLayout);

    std::vector<VkBuffer> uboBufferArray = {uboBufferMain->getBuffer()};
    engineSkybox = std::make_unique<EngineSkybox>(device, engineRenderer->getSwapChainRenderPass(), WIDTH, HEIGHT, *skyboxCubemap, globalSetLayout, uboBufferArray, sizeof(GlobalUbo));

    particleSystem = std::make_unique<EngineParticleSystem>(device, *engineRenderer, *descriptorManager, *uboBufferMain);
}

// ==========================================================
// 4. 메인 렌더 루프
// ==========================================================
void GameApp::run() {
    std::cout << "엔진 루프 진입 중..." << std::endl;
    auto currentTime = std::chrono::high_resolution_clock::now();
    float totalTime = 0.0f;
    const float targetFrameTime = 1.0f / 240.0f;

    // ★ FPS 측정을 위한 변수 추가
    int frameCount = 0;
    float timePassed = 0.0f;

    auto parallelFor = [&](size_t totalElements, size_t chunkSize, std::function<void(size_t, size_t)> action) {
        for (size_t i = 0; i < totalElements; i += chunkSize) {
            size_t end = std::min(i + chunkSize, totalElements);
            threadPool->enqueue([action, i, end]() {
                action(i, end); // 쪼개진 구간을 스레드가 실행
            });
        }
    };

    while (!window.shouldClose()) {
        window.pollEvents();

        // =======================================================
        // [1] 물리 및 로직 업데이트 (기존과 100% 동일)
        // =======================================================
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;
        totalTime += frameTime;

        frameCount++;
        timePassed += frameTime;
        if (timePassed >= 1.0f) {
            std::string title = "My Vulkan Engine | FPS: " + std::to_string(frameCount) 
                              + " | Frame Time: " + std::to_string(1000.0f / frameCount) + " ms";
            glfwSetWindowTitle(window.getGLFWwindow(), title.c_str());
            
            frameCount = 0;
            timePassed -= 1.0f; // 오차를 줄이기 위해 0 대신 1.0을 빼줍니다.
        }

        physicsEngine.update(frameTime);

        auto cameraView = registry.view<CameraTag, TransformComponent>();
        auto &viewTrans = cameraView.get<TransformComponent>(cameraView.front());

        auto playerView = registry.view<PlayerTag, TransformComponent>();
        auto &koroneTrans = playerView.get<TransformComponent>(playerView.front());
        
        cameraController.updateFreeCamera(window.getGLFWwindow(), frameTime, viewTrans);
        
        float yaw = viewTrans.rotation.y;
        float pitch = viewTrans.rotation.x;
        glm::vec3 lookDirection{-sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch)};
        camera.setViewTarget(viewTrans.translation, viewTrans.translation + lookDirection);
        camera.setPerspectiveProjection(glm::radians(50.f), engineRenderer->getAspectRatio(), 0.1f, 1000.f);

        // [2] UBO 갱신 (뼈대 및 조명 연산)
        GlobalUbo uboMain{};
        uboMain.view = camera.getView();
        uboMain.proj = camera.getProjection();
        uboMain.proj[1][1] *= -1.0f;
        uboMain.projectionView = uboMain.proj * uboMain.view;
        uboMain.time = totalTime;
        uboMain.lightDirection = glm::normalize(glm::vec3(0.5f, -3.0f, 1.0f));
        uboMain.lightSpaceMatrix = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 150.0f) * 
                                   glm::lookAt(-uboMain.lightDirection * 50.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uboMain.lightSpaceMatrix[1][1] *= -1.0f;

        auto ragdollView = registry.view<TransformComponent, RagdollComponent, ModelComponent>();
        for (auto entity : ragdollView) {
            auto &transform = ragdollView.get<TransformComponent>(entity);
            auto &ragdoll = ragdollView.get<RagdollComponent>(entity);
            auto &modelComp = ragdollView.get<ModelComponent>(entity);
            physicsEngine.syncRagdollBones(ragdoll.ragdollID, modelComp.model->getBoneInfoMap(), uboMain.finalBonesMatrices, transform.translation, transform.rotation);
        }

        auto animView = registry.view<AnimatorComponent>();
        std::vector<entt::entity> animEntities(animView.begin(), animView.end());
        
        if (!animEntities.empty()) {
            // 애니메이션은 연산이 무거우므로 10개 단위(Chunk)로 스레드에 분배합니다.
            parallelFor(animEntities.size(), 10, [&](size_t start, size_t end) {
                for (size_t i = start; i < end; ++i) {
                    auto entity = animEntities[i];
                    auto& animComp = animView.get<AnimatorComponent>(entity);
                    if (animComp.animator) {
                        animComp.animator->updateAnimation(frameTime); 
                    }
                }
            });
        }

        int lightCount = 0;
        auto lightView = registry.view<TransformComponent, PointLightComponent>();
        for (auto entity : lightView) {
            if (lightCount >= MAX_POINT_LIGHTS) break;
            auto &transform = lightView.get<TransformComponent>(entity);
            auto &pointLight = lightView.get<PointLightComponent>(entity);
            uboMain.pointLights[lightCount].position = glm::vec4(transform.translation, pointLight.intensity);
            uboMain.pointLights[lightCount].color = glm::vec4(pointLight.color, 1.0f); 
            lightCount++;
        }
        uboMain.numPointLights = lightCount;

        float waterHeight = 0.0f;
        auto waterView = registry.view<WaterComponent>();
        if (!waterView.empty()) {
            waterHeight = waterView.get<WaterComponent>(waterView.front()).height;
        }

        //프러스텀 컬링
        auto frustumPlanes = camera.getFrustumPlanes();
        auto cullView = registry.view<CullingComponent, BoundingSphereComponent, TransformComponent>();
        std::vector<entt::entity> cullEntities(cullView.begin(), cullView.end());

        if (!cullEntities.empty()) {
            // 컬링은 비교적 가벼운 수학 연산이므로 100개 단위로 묶어서 스레드에 던집니다.
            parallelFor(cullEntities.size(), 100, [&](size_t start, size_t end) {
                for (size_t j = start; j < end; ++j) {
                    auto entity = cullEntities[j];
                    auto& transform = cullView.get<TransformComponent>(entity);
                    auto& sphere = cullView.get<BoundingSphereComponent>(entity);
                    auto& cull = cullView.get<CullingComponent>(entity);

                    glm::vec3 center = glm::vec3(transform.mat4() * glm::vec4(sphere.offset, 1.0f));
                    float maxScale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
                    float radius = sphere.radius * maxScale * 1.5f; // 안전 마진!

                    cull.isVisible = true;
                    for (const auto& plane : frustumPlanes) {
                        if (glm::dot(plane.normal, center) + plane.distance < -radius) {
                            cull.isVisible = false; 
                            break;
                        }
                    }
                }
            });
        }
        
        // 모든 워커 스레드가 컬링 계산을 마칠 때까지 대기합니다.
        threadPool->waitAll();
        // =======================================================

        // =======================================================
        // [2-4] UBO 데이터 갱신 (메인 스레드)
        // =======================================================
        for (auto entity : animEntities) {
            auto& animComp = animView.get<AnimatorComponent>(entity);
            if (animComp.animator) {
                auto& transforms = animComp.animator->getFinalBoneMatrices();
                for (int i = 0; i < std::min((int)transforms.size(), MAX_BONES); i++) {
                    uboMain.finalBonesMatrices[i] = transforms[i];
                }
            }
        }

        //반사 및 굴절 카메라 UBO 세팅 (뼈대 데이터 포함)
        GlobalUbo uboRefraction = uboMain;
        uboRefraction.clipPlane = glm::vec4(0.0f, -1.0f, 0.0f, waterHeight + 0.1f);

        GlobalUbo uboReflection = uboMain;
        glm::vec3 refViewPos = viewTrans.translation;
        refViewPos.y -= 2.0f * (refViewPos.y - waterHeight); 
        glm::vec3 refTargetPos = koroneTrans.translation;
        refTargetPos.y -= 2.0f * (refTargetPos.y - waterHeight); 
        
        EngineCamera reflectionCamera{};
        reflectionCamera.setViewTarget(refViewPos, refTargetPos);
        uboReflection.view = reflectionCamera.getView();
        uboReflection.projectionView = uboReflection.proj * uboReflection.view;
        uboReflection.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, -waterHeight + 0.1f);

        uboBufferMain->writeToBuffer(&uboMain);
        uboBufferRefraction->writeToBuffer(&uboRefraction);
        uboBufferReflection->writeToBuffer(&uboReflection);


        // =======================================================
        // [3] 대망의 렌더링 시작! (복잡한 펜스, 이미지 획득이 증발했습니다!)
        // =======================================================
        if (auto commandBuffer = engineRenderer->beginFrame()) {
            //particle
            particleSystem->computeParticles(commandBuffer, frameTime);
            
            // --- 패스 1: 그림자 렌더링 (Shadow) ---
            VkRenderPassBeginInfo shadowPassInfo{};
            shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            shadowPassInfo.renderPass = engineShadow->getRenderPass();
            shadowPassInfo.framebuffer = engineShadow->getFramebuffer();
            shadowPassInfo.renderArea.extent = {engineShadow->getWidth(), engineShadow->getHeight()};
            VkClearValue depthClear{}; depthClear.depthStencil = {1.0f, 0};
            shadowPassInfo.clearValueCount = 1; shadowPassInfo.pClearValues = &depthClear;

            vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            
            shadowSystem->render(commandBuffer, registry);
            vkCmdEndRenderPass(commandBuffer);

            // --- 패스 1.5: 반사 렌더링 (Reflection) ---
            VkRenderPassBeginInfo reflectionPassInfo{};
            reflectionPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            reflectionPassInfo.renderPass = engineWater->getReflectionRenderPass();
            reflectionPassInfo.framebuffer = engineWater->getReflectionFramebuffer();
            reflectionPassInfo.renderArea.extent = {engineWater->getWidth(), engineWater->getHeight()};
            VkClearValue refColor = {{{0.5f, 0.7f, 0.9f, 1.0f}}};
            std::vector<VkClearValue> refClearValues = {refColor, depthClear};
            reflectionPassInfo.clearValueCount = static_cast<uint32_t>(refClearValues.size());
            reflectionPassInfo.pClearValues = refClearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &reflectionPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            simpleRenderSystem->renderGameObjects(commandBuffer, registry, RenderPassType::REFLECTION);
            engineSkybox->render(commandBuffer, 0); 
            vkCmdEndRenderPass(commandBuffer);

            // --- 패스 1.6: 굴절 렌더링 (Refraction) ---
            VkRenderPassBeginInfo refractionPassInfo = reflectionPassInfo;
            refractionPassInfo.renderPass = engineWater->getRefractionRenderPass();
            refractionPassInfo.framebuffer = engineWater->getRefractionFramebuffer();

            vkCmdBeginRenderPass(commandBuffer, &refractionPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            simpleRenderSystem->renderGameObjects(commandBuffer, registry, RenderPassType::REFRACTION);
            
            engineSkybox->render(commandBuffer, 0);

            vkCmdEndRenderPass(commandBuffer);

            // =======================================================
            // --- 패스 2: 메인 화면 렌더링 (다이어트 핵심 구간!) ---
            // =======================================================
            // ★ 그 많던 스왑체인 버퍼 클리어 및 세팅이 단 한 줄로 압축되었습니다!
            engineRenderer->beginSwapChainRenderPass(commandBuffer);
            
            // 1. 코로네 & 바닥
            simpleRenderSystem->renderGameObjects(commandBuffer, registry);
            
            // 2. 스카이박스
            engineSkybox->render(commandBuffer, 0);

            //2.5 particle
            particleSystem->renderParticles(commandBuffer);
            
            // 3. 물
            waterRenderSystem->render(commandBuffer, registry);

            // ★ 메인 무대 닫기!
            engineRenderer->endSwapChainRenderPass(commandBuffer);

            // =======================================================
            // [4] 화면 출력 제출 (이 한 줄이 QueueSubmit, Present 등을 다 해줍니다)
            // =======================================================
            engineRenderer->endFrame();
        }
    }
}


// ==========================================================
// ★ 데이터 기반 씬 로더 
// ==========================================================
void GameApp::loadSceneFromJSON(const std::string& filepath) {
    std::cout << "JSON 씬 로딩 중: " << filepath << std::endl;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("JSON 씬 파일을 찾을 수 없습니다: " + filepath);
    }

    json j;
    file >> j;

    //0. skybox 로드
    std::string skyboxPath = "../textures/sunflowers_puresky_4k.hdr"; // 기본값
    if (j.contains("environment") && j.contains("skybox")) {
        skyboxPath = j;
    }
    std::cout << "  - 스카이박스 로드 완료: " << skyboxPath << std::endl;
    EngineTexture hdrSkyboxTexture{device};
    hdrSkyboxTexture.loadHDR(skyboxPath);
    skyboxCubemap = std::make_unique<EngineCubemap>(device, hdrSkyboxTexture, 4096);

    // 1. 필수 에셋 로딩 (models & textures)
    if (j.contains("required_assets")) {
        for (const auto& modelData : j["required_assets"]["models"]) {
            std::string name = modelData["name"];
            std::string path = modelData["path"];
            assetManager->loadModel(name, path);
            std::cout << "  - 모델 로드 완료: " << name << std::endl;
        }
        for (const auto& texData : j["required_assets"]["textures"]) {
            std::string name = texData["name"];
            std::string path = texData["path"];
            assetManager->loadTexture(name, path);
            std::cout << "  - 텍스처 로드 완료: " << name << std::endl;
        }
    }

    // 2. 엔티티 스폰 (ECS 구성)
    if (j.contains("entities")) {
        for (const auto& entityData : j["entities"]) {
            std::string tag = entityData["tag"];
            
            // 1. 트랜스폼 데이터 추출 및 장착 (모든 엔티티 공통)
            glm::vec3 pos{0.0f}, scale{1.0f}, rot{0.0f};
            if (entityData.contains("transform")) {
                auto& t = entityData["transform"];
                pos = glm::vec3(t["position"][0], t["position"][1], t["position"][2]);
                scale = glm::vec3(t["scale"][0], t["scale"][1], t["scale"][2]);
                if (t.contains("rotation")) {
                    rot = glm::vec3(glm::radians((float)t["rotation"][0]), 
                                    glm::radians((float)t["rotation"][1]), 
                                    glm::radians((float)t["rotation"][2]));
                }
            }

            auto entity = registry.create();
            auto& transform = registry.emplace<TransformComponent>(entity);
            transform.translation = pos;
            transform.scale = scale;

            // 2. 모델이 있는 경우만 ModelComponent 장착
            if (entityData.contains("model")) {
                std::string modelName = entityData["model"];
                // ★ map.find() 대신 그냥 try-catch로 안전하게 가져오거나 예외처리
                try {
                    auto model = assetManager->getModel(modelName);
                    auto& modelComp = registry.emplace<ModelComponent>(entity, model);
                    modelComp.roughness = 0.8f; 


                    std::string texName = "Wood"; // 기본값
                    if (entityData.contains("texture")) {
                        texName = entityData["texture"]; // 안전한 추출
                    }
                    registry.emplace<MaterialComponent>(entity, texName);

                    float radius = modelComp.model->getBoundingRadius();
                    glm::vec3 center = modelComp.model->getBoundingCenter();

                    registry.emplace<BoundingSphereComponent>(entity, radius, center); 
                    registry.emplace<CullingComponent>(entity);

                    if (entityData.contains("animations")) {
                        auto animData = entityData["animations"];
                        
                        // 현재는 기본 대기(idle) 모션만 로드해서 재생하도록 세팅합니다.
                        if (animData.contains("idle")) {
                            std::string idlePath = animData["idle"].get<std::string>();
                            
                            // EngineAnimation은 해당 모델의 뼈대(Bone) 구조를 알아야 하므로 model 포인터를 넘겨줍니다.
                            auto idleAnim = std::make_shared<EngineAnimation>(idlePath, model.get());
                            
                            // AnimatorComponent 생성자를 통해 애니메이션 세팅!
                            registry.emplace<AnimatorComponent>(entity, idleAnim);
                            
                            auto& sphere = registry.get<BoundingSphereComponent>(entity);
                            sphere.radius *= 100.0f;

                            std::cout << "  - 애니메이션 로드 및 부착 완료: " << idlePath << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "경고: " << e.what() << std::endl;
                }
            }

            // 3. 태그에 따른 특수 컴포넌트 장착
            if (tag == "Player") {
                registry.emplace<PlayerTag>(entity);
                //uint32_t ragdollID = physicsEngine.createSimpleRagdoll(pos);
                //registry.emplace<RagdollComponent>(entity, ragdollID);
            } 
            else if (tag == "Floor") {
                registry.emplace<FloorTag>(entity);
            }
            else if (tag == "Prop") {
                registry.emplace<PropTag>(entity); // 일반 사물 명찰!
            }
            else if (tag == "Camera") {
                registry.emplace<CameraTag>(entity);
            }
            else if (tag == "Light") {
                glm::vec3 color{1.0f};
                float intensity = 100.0f;
                if (entityData.contains("light")) {
                    auto& l = entityData["light"];
                    color = glm::vec3(l["color"][0], l["color"][1], l["color"][2]);
                    intensity = l["intensity"];
                }
                registry.emplace<PointLightComponent>(entity, color, intensity);
            }
            else if (tag == "Water") {
                auto& water = registry.emplace<WaterComponent>(entity);
                if (entityData.contains("water_properties")) {
                    auto& wp = entityData;
                    water.height = wp.value("height", 0.5f);
                    water.waveSpeed = wp.value("waveSpeed", 0.05f);
                    water.dudvTexture = wp.value("dudvTexture", "WaterDUDV");
                    water.normalTexture = wp.value("normalTexture", "WaterNormal");
                }
            }
        }
    }



    // ★ 수정 4: 모든 에셋과 ECS 엔티티 세팅이 끝났으므로, 디스크립터를 조립합니다!
    setupDescriptorsAndPipelines();
    
    // (선택) 조명이나 카메라도 JSON에서 읽어오도록 확장할 수 있습니다.
    std::cout << "씬 로딩 완료: " << j["scene_name"] << std::endl;
}