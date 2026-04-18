#include "GameApp.hpp"
#include <iostream>
#include <chrono>
#include <fstream>
#include <cmath>


using json = nlohmann::json;

//엔진 초기화
GameApp::GameApp() {
    std::cout << "엔진 코어 초기화 중..." << std::endl;
    
    assetManager = std::make_unique<AssetManager>(device); 
    threadPool = std::make_unique<EngineThreadPool>();
    engineRenderer = std::make_unique<EngineRenderer>(window, device);

    // 물리 엔진 초기화
    physicsEngine.init();
    physicsEngine.createFloor();

    // UBO 버퍼 생성 및 매핑
    uboBuffersMain.resize(MAX_FRAMES_IN_FLIGHT);
    uboBuffersReflection.resize(MAX_FRAMES_IN_FLIGHT);
    uboBuffersRefraction.resize(MAX_FRAMES_IN_FLIGHT);
    boneSSBOs.resize(MAX_FRAMES_IN_FLIGHT);

    uint32_t maxCharacters = 1000;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uboBuffersMain[i] = std::make_unique<EngineBuffer>(device, sizeof(GlobalUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uboBuffersMain[i]->map();
        
        uboBuffersReflection[i] = std::make_unique<EngineBuffer>(device, sizeof(GlobalUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uboBuffersReflection[i]->map();
        
        uboBuffersRefraction[i] = std::make_unique<EngineBuffer>(device, sizeof(GlobalUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uboBuffersRefraction[i]->map();

        boneSSBOs[i] = std::make_unique<EngineBuffer>(
            device, 
            sizeof(glm::mat4) * MAX_BONES * maxCharacters, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        boneSSBOs[i]->map();
    }

    // 텍스처 및 스카이박스 로딩
    std::cout << "코어 에셋 로딩 중..." << std::endl;

    engineWater = std::make_unique<EngineWater>(device, WIDTH, HEIGHT);
    engineShadow = std::make_unique<EngineShadow>(device, 2048, 2048);
    descriptorManager = std::make_unique<EngineDescriptorManager>(device);
}

GameApp::~GameApp() {
    vkDeviceWaitIdle(device.getDevice());
    std::cout << "엔진 정상 종료 및 메모리 정리 완료." << std::endl;
}

//디스크립터 조립 및 파이프라인 생성
void GameApp::setupDescriptorsAndPipelines() {
    std::cout << "파이프라인 및 디스크립터 세팅 중..." << std::endl;

    // 레이아웃 생성
    std::vector<VkDescriptorSetLayoutBinding> globalBindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // ★ [NEW] 4번: 조도 맵(Irradiance)
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}, // ★ [NEW] 뼈대 SSBO (5번 바인딩)
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    globalSetLayout = descriptorManager->createDescriptorSetLayout(globalBindings);

    std::vector<VkDescriptorSetLayoutBinding> waterBindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, 
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, 
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, 
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},  
        {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    waterSetLayout = descriptorManager->createDescriptorSetLayout(waterBindings);

    // 이미지 정보 묶기
    auto makeImgInfo = [](VkImageLayout layout, VkImageView view, VkSampler sampler) {
        VkDescriptorImageInfo info{}; info.imageLayout = layout; info.imageView = view; info.sampler = sampler; return info;
    };    
    auto shadowImageInfo = makeImgInfo(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, engineShadow->getImageView(), engineShadow->getSampler());
    auto skyboxInfo      = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, skyboxCubemap->getImageView(), skyboxCubemap->getSampler());
    auto irradianceInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, skyboxCubemap->getIrradianceImageView(), skyboxCubemap->getSampler());
    auto reflectionInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, engineWater->getReflectionImageView(), engineWater->getSampler());
    auto refractionInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, engineWater->getRefractionImageView(), engineWater->getSampler());
    auto prefilterInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, skyboxCubemap->getPrefilteredImageView(), skyboxCubemap->getSampler());  
    
    // 디스크립터 세트 조립
    auto renderableView = registry.view<ModelComponent, MaterialComponent>();
    for (auto entity : renderableView) {
        auto &modelComp = renderableView.get<ModelComponent>(entity);
        auto &matComp = renderableView.get<MaterialComponent>(entity);

        modelComp.mainSets.resize(MAX_FRAMES_IN_FLIGHT);
        modelComp.reflectionSets.resize(MAX_FRAMES_IN_FLIGHT);
        modelComp.refractionSets.resize(MAX_FRAMES_IN_FLIGHT);

        auto albedoTex = assetManager->getTexture(matComp.albedoTexture);
        auto albedoInfo = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, albedoTex->getImageView(), albedoTex->getSampler());

        std::string normalTexName = "DefaultNormal"; 
        
        auto normalTex = assetManager->getTexture(normalTexName);
        auto normalInfo = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, normalTex->getImageView(), normalTex->getSampler());

        // 2개의 프레임 각각에 대해 UBO와 SSBO 정보 세팅
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            auto uboInfoMain       = uboBuffersMain[i]->descriptorInfo();
            auto uboInfoReflection = uboBuffersReflection[i]->descriptorInfo();
            auto uboInfoRefraction = uboBuffersRefraction[i]->descriptorInfo();
            VkDescriptorBufferInfo boneInfo{boneSSBOs[i]->getBuffer(), 0, VK_WHOLE_SIZE};

            EngineDescriptorManager::Builder(*descriptorManager)
                .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
                .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &albedoInfo) 
                .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
                .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &skyboxInfo)
                .bindImage(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &irradianceInfo)
                .bindBuffer(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, &boneInfo)
                .bindImage(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &prefilterInfo)
                .bindImage(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &normalInfo)
                .build(modelComp.mainSets[i]);

            EngineDescriptorManager::Builder(*descriptorManager)
                .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoReflection)
                .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &albedoInfo)
                .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
                .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &skyboxInfo)
                .bindImage(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &irradianceInfo)
                .bindBuffer(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, &boneInfo)
                .bindImage(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &prefilterInfo)
                .bindImage(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &normalInfo)
                .build(modelComp.reflectionSets[i]);

            EngineDescriptorManager::Builder(*descriptorManager)
                .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoRefraction)
                .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &albedoInfo)
                .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
                .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &skyboxInfo)
                .bindImage(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &irradianceInfo)
                .bindBuffer(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, &boneInfo)
                .bindImage(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &prefilterInfo)
                .bindImage(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &normalInfo)
                .build(modelComp.refractionSets[i]);
        }
    }

    auto waterView = registry.view<WaterComponent>();
    for (auto entity : waterView) {
        auto& water = waterView.get<WaterComponent>(entity);
        
        // 물 전용 디스크립터 배열 크기 확보
        water.waterSets.resize(MAX_FRAMES_IN_FLIGHT);
        
        auto dudvTex   = assetManager->getTexture(water.dudvTexture);
        auto normalTex = assetManager->getTexture(water.normalTexture);

        auto dudvInfo   = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, dudvTex->getImageView(), dudvTex->getSampler());
        auto normalInfo = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, normalTex->getImageView(), normalTex->getSampler());
        auto depthInfo  = makeImgInfo(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, engineWater->getRefractionDepthView(), engineWater->getSampler());

        // 2개의 프레임 각각에 대해 조립
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            auto uboInfoMain = uboBuffersMain[i]->descriptorInfo(); // 물 렌더링 시에는 메인 UBO를 사용

            EngineDescriptorManager::Builder(*descriptorManager)
                .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
                .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &reflectionInfo)
                .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &refractionInfo)
                .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &dudvInfo)   
                .bindImage(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &normalInfo) 
                .bindImage(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &depthInfo)
                .build(water.waterSets[i]);
        }
    }


    // 파이프라인 생성
    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(device, engineRenderer->getSwapChainRenderPass(), globalSetLayout);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    shadowSystem = std::make_unique<EngineShadowSystem>(device, engineShadow->getRenderPass(), globalSetLayout);
    waterRenderSystem = std::make_unique<EngineWaterSystem>(device, engineRenderer->getSwapChainRenderPass(), waterSetLayout);

    // 스카이박스: 2개의 프레임 버퍼를 모두 배열로 전달
    std::vector<VkBuffer> uboBufferArray(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uboBufferArray[i] = uboBuffersMain[i]->getBuffer();
    }

    engineSkybox = std::make_unique<EngineSkybox>(
        device, 
        engineRenderer->getSwapChainRenderPass(), 
        WIDTH, HEIGHT, 
        *skyboxCubemap, 
        globalSetLayout, 
        uboBufferArray,
        sizeof(GlobalUbo)
    );


    // 파티클 시스템: 전체 버퍼 배열을 전달
    particleSystem = std::make_unique<EngineParticleSystem>(
        device, 
        *engineRenderer, 
        *descriptorManager, 
        uboBuffersMain
    );

}

//메인 렌더 루프
void GameApp::run() {
    std::cout << "엔진 루프 진입 중..." << std::endl;
    auto currentTime = std::chrono::high_resolution_clock::now();
    float totalTime = 0.0f;

    int frameCount = 0;
    float timePassed = 0.0f;

    auto parallelFor = [&](size_t totalElements, size_t chunkSize, std::function<void(size_t, size_t)> action) {
        for (size_t i = 0; i < totalElements; i += chunkSize) {
            size_t end = std::min(i + chunkSize, totalElements);
            threadPool->enqueue([action, i, end]() { action(i, end); });
        }
    };

    while (!window.shouldClose()) {
        window.pollEvents();

        // 물리 및 로직 업데이트 
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;
        totalTime += frameTime;

        frameCount++;
        timePassed += frameTime;
        if (timePassed >= 1.0f) {
            std::string title = "My Vulkan Engine | FPS: " + std::to_string(frameCount) + " | Frame Time: " + std::to_string(1000.0f / frameCount) + " ms";
            glfwSetWindowTitle(window.getGLFWwindow(), title.c_str());
            frameCount = 0;
            timePassed -= 1.0f; 
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

        // 다중 프레임 렌더링 시작
        if (auto commandBuffer = engineRenderer->beginFrame()) {
            int frameIndex = currentFrame;

            // UBO 데이터 계산
            GlobalUbo uboMain{};
            uboMain.view = camera.getView();
            uboMain.proj = camera.getProjection();
            uboMain.proj[1][1] *= -1.0f;
            uboMain.projectionView = uboMain.proj * uboMain.view;
            uboMain.time = totalTime;
            uboMain.clipPlane = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            uboMain.lightDirection = glm::normalize(glm::vec3(0.5f, -3.0f, 1.0f));

            glm::mat4 ortho = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 200.0f);
            ortho[1][1] *= -1.0f;
            ortho[2][2] *= 0.5f;
            ortho[3][2] += 0.5f;

            glm::mat4 lightView_ortho = glm::lookAt(
                -uboMain.lightDirection * 30.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f)
            );
            uboMain.lightSpaceMatrix = ortho * lightView_ortho;

            uboMain.lightSpaceMatrix[1][1] *= -1.0f;

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

            //멀티스레드 연산 (애니메이션, 컬링)
            auto animView = registry.view<AnimatorComponent>();
            std::vector<entt::entity> animEntities(animView.begin(), animView.end());
            if (!animEntities.empty()) {
                parallelFor(animEntities.size(), 10, [&](size_t start, size_t end) {
                    for (size_t i = start; i < end; ++i) {
                        auto entity = animEntities[i];
                        auto& animComp = animView.get<AnimatorComponent>(entity);
                        if (animComp.animator) animComp.animator->updateAnimation(frameTime); 
                    }
                });
            }

            auto frustumPlanes = camera.getFrustumPlanes();
            auto cullView = registry.view<CullingComponent, BoundingSphereComponent, TransformComponent>();
            std::vector<entt::entity> cullEntities(cullView.begin(), cullView.end());
            if (!cullEntities.empty()) {
                parallelFor(cullEntities.size(), 100, [&](size_t start, size_t end) {
                    for (size_t j = start; j < end; ++j) {
                        auto entity = cullEntities[j];
                        auto& transform = cullView.get<TransformComponent>(entity);
                        auto& sphere = cullView.get<BoundingSphereComponent>(entity);
                        auto& cull = cullView.get<CullingComponent>(entity);
                        glm::vec3 center = glm::vec3(transform.mat4() * glm::vec4(sphere.offset, 1.0f));
                        float maxScale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
                        float radius = sphere.radius * maxScale * 1.5f; 
                        cull.isVisible = true;
                        for (const auto& plane : frustumPlanes) {
                            if (glm::dot(plane.normal, center) + plane.distance < -radius) {
                                cull.isVisible = false; break;
                            }
                        }
                    }
                });
            }
            
            threadPool->waitAll(); // 스레드 연산 완료 대기

            //버퍼 데이터 쓰기 (frameIndex 배열 슬롯에 저장)
            int currentCharacterIndex = 0; 
            for (auto entity : animEntities) {
                auto& animComp = animView.get<AnimatorComponent>(entity);
                if (animComp.animator) {
                    const auto& transforms = animComp.animator->getFinalBoneMatrices();
                    //현재 프레임의 SSBO에만 기록
                    boneSSBOs[frameIndex]->writeToBuffer((void*)transforms.data(), sizeof(glm::mat4) * transforms.size(), sizeof(glm::mat4) * MAX_BONES * currentCharacterIndex);
                    animComp.characterIndex = currentCharacterIndex;
                    currentCharacterIndex++;
                }
            }

            float waterHeight = 0.0f;
            auto waterView = registry.view<WaterComponent>();
            if (!waterView.empty()) waterHeight = waterView.get<WaterComponent>(waterView.front()).height;

            GlobalUbo uboRefraction = uboMain;
            uboRefraction.clipPlane = glm::vec4(0.0f, -1.0f, 0.0f, waterHeight + 0.1f);

            GlobalUbo uboReflection = uboMain;
            
            // 카메라 위치를 물 아래로 대칭 이동
            glm::vec3 refViewPos = viewTrans.translation;
            refViewPos.y -= 2.0f * (refViewPos.y - waterHeight); 
            
            // 메인 카메라의 각도
            float yaw = viewTrans.rotation.y;
            float pitch = viewTrans.rotation.x;
            
            // 시선 방향에서 Y축만 반전
            glm::vec3 refLookDirection{-sin(yaw) * cos(pitch), -sin(pitch), -cos(yaw) * cos(pitch)};
            
            //대칭된 위치에서 반전된 시선 방향
            EngineCamera reflectionCamera{};
            reflectionCamera.setViewTarget(refViewPos, refViewPos + refLookDirection);
            
            uboReflection.view = reflectionCamera.getView();
            uboReflection.projectionView = uboReflection.proj * uboReflection.view;
            uboReflection.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, -waterHeight + 0.1f);

            // 현재 프레임의 UBO에만 기록
            uboBuffersMain[frameIndex]->writeToBuffer(&uboMain);
            uboBuffersRefraction[frameIndex]->writeToBuffer(&uboRefraction);
            uboBuffersReflection[frameIndex]->writeToBuffer(&uboReflection);
            
            // 그림자 패스
            VkRenderPassBeginInfo shadowPassInfo{};
            shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            shadowPassInfo.renderPass = engineShadow->getRenderPass();
            shadowPassInfo.framebuffer = engineShadow->getFramebuffer();
            shadowPassInfo.renderArea.extent = {engineShadow->getWidth(), engineShadow->getHeight()};
            VkClearValue depthClear{}; depthClear.depthStencil = {1.0f, 0};
            shadowPassInfo.clearValueCount = 1; shadowPassInfo.pClearValues = &depthClear;

            vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            shadowSystem->render(commandBuffer, registry, frameIndex);
            vkCmdEndRenderPass(commandBuffer);

            // 반사 패스
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
            simpleRenderSystem->renderGameObjects(commandBuffer, registry, RenderPassType::REFLECTION, frameIndex);
            engineSkybox->render(commandBuffer, frameIndex); 
            vkCmdEndRenderPass(commandBuffer);

            // 굴절 패스
            VkRenderPassBeginInfo refractionPassInfo = reflectionPassInfo;
            refractionPassInfo.renderPass = engineWater->getRefractionRenderPass();
            refractionPassInfo.framebuffer = engineWater->getRefractionFramebuffer();

            vkCmdBeginRenderPass(commandBuffer, &refractionPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            simpleRenderSystem->renderGameObjects(commandBuffer, registry, RenderPassType::REFRACTION, frameIndex);
            vkCmdEndRenderPass(commandBuffer);

            // --- 메인 패스 ---
            engineRenderer->beginSwapChainRenderPass(commandBuffer);
            simpleRenderSystem->renderGameObjects(commandBuffer, registry, RenderPassType::MAIN, frameIndex);
            engineSkybox->render(commandBuffer, frameIndex);
            particleSystem->renderParticles(commandBuffer, frameIndex);
            waterRenderSystem->render(commandBuffer, registry, frameIndex);
            engineRenderer->endSwapChainRenderPass(commandBuffer);

            engineRenderer->endFrame();
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
    }
}

// json 데이터 기반 씬 로더 
void GameApp::loadSceneFromJSON(const std::string& filepath) {
    std::cout << "JSON 씬 로딩 중: " << filepath << std::endl;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("JSON 씬 파일을 찾을 수 없습니다: " + filepath);
    }

    json j;
    file >> j;

    // skybox 로드
    std::string skyboxPath = "../textures/sunflowers_puresky_4k.hdr"; // 기본값
    if (j.contains("environment") && j.contains("skybox")) {
        skyboxPath = j;
    }
    std::cout << "  - 스카이박스 로드 완료: " << skyboxPath << std::endl;
    EngineTexture hdrSkyboxTexture{device};
    hdrSkyboxTexture.loadHDR(skyboxPath);
    skyboxCubemap = std::make_unique<EngineCubemap>(device, hdrSkyboxTexture, 4096);

    // 필수 에셋 로딩 
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

    // 엔티티 스폰 (ECS 구성)
    if (j.contains("entities")) {
        for (const auto& entityData : j["entities"]) {
            std::string tag = entityData["tag"];
            
            // 트랜스폼 데이터 추출 및 장착 (모든 엔티티 공통)
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

            // 모델이 있는 경우만 ModelComponent 장착
            if (entityData.contains("model")) {
                std::string modelName = entityData["model"];
                try {
                    auto model = assetManager->getModel(modelName);
                    auto& modelComp = registry.emplace<ModelComponent>(entity, model);
                    

                    modelComp.roughness = 0.8f; // 기본값 (거친 질감)
                    modelComp.metallic = 0.0f;  // 기본값 (비금속)
                    
                    if (entityData.contains("material")) {
                        auto& matData = entityData["material"];
                        modelComp.roughness = matData.value("roughness", 0.8f);
                        modelComp.metallic  = matData.value("metallic", 0.0f);
                    }


                    std::string texName = "Wood"; // 기본값
                    if (entityData.contains("texture")) {
                        texName = entityData["texture"];
                    }
                    registry.emplace<MaterialComponent>(entity, texName);

                    float radius = modelComp.model->getBoundingRadius();
                    glm::vec3 center = modelComp.model->getBoundingCenter();

                    registry.emplace<BoundingSphereComponent>(entity, radius, center); 
                    registry.emplace<CullingComponent>(entity);

                    if (entityData.contains("animations")) {
                        auto animData = entityData["animations"];
                        
                        // 기본 대기(idle) 모션 로드
                        if (animData.contains("idle")) {
                            std::string idlePath = animData["idle"].get<std::string>();
                            
                            auto idleAnim = std::make_shared<EngineAnimation>(idlePath, model.get());
                            
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

            // 태그에 따른 특수 컴포넌트 장착
            if (tag == "Player") {
                registry.emplace<PlayerTag>(entity);
                //래그돌 사용시 위 코드 주석처리
                //uint32_t ragdollID = physicsEngine.createSimpleRagdoll(pos);
                //registry.emplace<RagdollComponent>(entity, ragdollID);
            } 
            else if (tag == "Floor") {
                registry.emplace<FloorTag>(entity);
            }
            else if (tag == "Prop") {
                registry.emplace<PropTag>(entity);
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



    // 디스크립터 조립
    setupDescriptorsAndPipelines();
    
    std::cout << "씬 로딩 완료: " << j["scene_name"] << std::endl;
}