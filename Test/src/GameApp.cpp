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
    assetManager->loadTexture("WaterDUDV", "../textures/waterDUDV.png");
    assetManager->loadTexture("WaterNormal", "../textures/waterNormal.jpg");

    EngineTexture hdrSkyboxTexture{device};
    hdrSkyboxTexture.loadHDR("../textures/sunflowers_puresky_4k.hdr");
    skyboxCubemap = std::make_unique<EngineCubemap>(device, hdrSkyboxTexture, 4096);

    engineWater = std::make_unique<EngineWater>(device, WIDTH, HEIGHT);
    engineShadow = std::make_unique<EngineShadow>(device, 2048, 2048);
    descriptorManager = std::make_unique<EngineDescriptorManager>(device);

    //particle
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto& particle : particles) {
        particle.position = glm::vec3(0.0f, 10.0f, 0.0f); // 코로네 위쪽에서 스폰
        particle.velocity = glm::vec3((rand() % 100 - 50) * 0.1f, (rand() % 100) * 0.1f, (rand() % 100 - 50) * 0.1f);
        particle.color = glm::vec4(1.0f, (rand() % 100) * 0.01f, 0.2f, 1.0f);
    }

    particleSSBO = std::make_unique<EngineBuffer>(
        device, sizeof(Particle) * PARTICLE_COUNT, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    particleSSBO->map();
    particleSSBO->writeToBuffer(particles.data());

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

    auto koroneTex = assetManager->getTexture("KoroneMap");
    auto woodTex   = assetManager->getTexture("Wood");
    auto dudvTex   = assetManager->getTexture("WaterDUDV");
    auto normalTex = assetManager->getTexture("WaterNormal");

    auto koroneImageInfo = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, koroneTex->getImageView(), koroneTex->getSampler());
    auto floorImageInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, woodTex->getImageView(), woodTex->getSampler());
    
    auto shadowImageInfo = makeImgInfo(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, engineShadow->getImageView(), engineShadow->getSampler());
    auto skyboxInfo      = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, skyboxCubemap->getImageView(), skyboxCubemap->getSampler());
    auto reflectionInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, engineWater->getReflectionImageView(), engineWater->getSampler());
    auto refractionInfo  = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, engineWater->getRefractionImageView(), engineWater->getSampler());
    
    // 코어 에셋 창고에서 꺼내기
    auto dudvInfo   = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, dudvTex->getImageView(), dudvTex->getSampler());
    auto normalInfo = makeImgInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, normalTex->getImageView(), normalTex->getSampler());    
    
    // 버퍼 정보
    auto uboInfoMain = uboBufferMain->descriptorInfo();
    auto uboInfoReflection = uboBufferReflection->descriptorInfo();
    auto uboInfoRefraction = uboBufferRefraction->descriptorInfo();

    // ECS에 저장된 모델 컴포넌트 꺼내오기
    auto playerView = registry.view<PlayerTag, ModelComponent>();
    auto &kComp = registry.get<ModelComponent>(playerView.front());

    auto floorView = registry.view<FloorTag, ModelComponent>();
    auto &fComp = registry.get<ModelComponent>(floorView.front());

    // 디스크립터 세트 조립
    auto propView = registry.view<PropTag, ModelComponent>();
    for (auto entity : propView) {
        auto &pComp = propView.get<ModelComponent>(entity);
        
        EngineDescriptorManager::Builder(*descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
            .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &floorImageInfo) // 임시로 나무 텍스처 사용
            .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
            .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &skyboxInfo)
            .build(pComp.mainSet);

        EngineDescriptorManager::Builder(*descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoReflection)
            .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &floorImageInfo)
            .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
            .build(pComp.reflectionSet);

        EngineDescriptorManager::Builder(*descriptorManager)
            .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoRefraction)
            .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &floorImageInfo)
            .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
            .build(pComp.refractionSet);
    }


    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
        .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &koroneImageInfo)
        .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
        .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &skyboxInfo)
        .build(kComp.mainSet);

    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoReflection)
        .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &koroneImageInfo)
        .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
        .build(kComp.reflectionSet);

    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoRefraction)
        .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &koroneImageInfo)
        .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
        .build(kComp.refractionSet);

    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
        .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &floorImageInfo)
        .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
        .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &skyboxInfo)
        .build(fComp.mainSet);

    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoReflection)
        .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &floorImageInfo)
        .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
        .build(fComp.reflectionSet);

    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoRefraction)
        .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &floorImageInfo)
        .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &shadowImageInfo)
        .build(fComp.refractionSet);

    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &uboInfoMain)
        .bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &reflectionInfo)
        .bindImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &refractionInfo)
        .bindImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &dudvInfo)   
        .bindImage(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &normalInfo) 
        .build(waterSet);

    // 파이프라인 생성!
    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(device, engineRenderer->getSwapChainRenderPass(), globalSetLayout);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    PipelineConfigInfo shadowPipelineConfig{};
    EnginePipeline::defaultPipelineConfigInfo(shadowPipelineConfig, engineShadow->getWidth(), engineShadow->getHeight());
    shadowPipelineConfig.colorBlendInfo.attachmentCount = 0;
    shadowPipelineConfig.colorBlendInfo.pAttachments = nullptr;
    shadowPipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
    shadowPipelineConfig.renderPass = engineShadow->getRenderPass(); 
    shadowPipelineConfig.descriptorSetLayouts = {globalSetLayout};
    shadowPipelineConfig.pushConstantRanges = {pushConstantRange};
    shadowPipeline = std::make_unique<EnginePipeline>(device, "../Test/shaders/shadow.vert.spv", "../Test/shaders/shadow.frag.spv", shadowPipelineConfig);

    PipelineConfigInfo waterPipelineConfig{};
    EnginePipeline::defaultPipelineConfigInfo(waterPipelineConfig, WIDTH, HEIGHT);
    waterPipelineConfig.renderPass = engineRenderer->getSwapChainRenderPass();
    waterPipelineConfig.descriptorSetLayouts = {waterSetLayout};
    waterPipelineConfig.pushConstantRanges = {pushConstantRange};
    waterPipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; 
    waterPipeline = std::make_unique<EnginePipeline>(device, "../Test/shaders/water.vert.spv", "../Test/shaders/water.frag.spv", waterPipelineConfig);

    std::vector<VkBuffer> uboBufferArray = {uboBufferMain->getBuffer()};
    engineSkybox = std::make_unique<EngineSkybox>(device, engineRenderer->getSwapChainRenderPass(), WIDTH, HEIGHT, *skyboxCubemap, globalSetLayout, uboBufferArray, sizeof(GlobalUbo));

    //particle
    std::vector<VkDescriptorSetLayoutBinding> computeBindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    computeSetLayout = descriptorManager->createDescriptorSetLayout(computeBindings);

    auto computeUboInfo = uboBufferMain->descriptorInfo(); 
    VkDescriptorBufferInfo ssboInfo{particleSSBO->getBuffer(), 0, VK_WHOLE_SIZE};

    EngineDescriptorManager::Builder(*descriptorManager)
        .bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, &computeUboInfo)
        .bindBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, &ssboInfo)
        .build(computeDescriptorSet);

    // 2. 컴퓨트 파이프라인 생성 (dt 푸시 상수 포함)
    VkPushConstantRange computePush{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)};
    VkPipelineLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayoutInfo.setLayoutCount = 1;
    computeLayoutInfo.pSetLayouts = &computeSetLayout;
    computeLayoutInfo.pushConstantRangeCount = 1;
    computeLayoutInfo.pPushConstantRanges = &computePush;
    vkCreatePipelineLayout(device.getDevice(), &computeLayoutInfo, nullptr, &computePipelineLayout);
    
    computePipeline = std::make_unique<EnginePipeline>(device, "../Test/shaders/particle.comp.spv", computePipelineLayout);

    // 3. 파티클 그래픽스 파이프라인 생성 (푸시 상수 없음!)
    PipelineConfigInfo particleConfig{};
    EnginePipeline::defaultPipelineConfigInfo(particleConfig, WIDTH, HEIGHT);
    particleConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; 
    particleConfig.attributeDescriptions.clear(); 
    particleConfig.bindingDescriptions.clear();
    particleConfig.pushConstantRanges.clear(); // ★그래픽스는 푸시 상수 안 씀!
    particleConfig.renderPass = engineRenderer->getSwapChainRenderPass();
    particleConfig.descriptorSetLayouts = {computeSetLayout}; // 메뉴판 공유!

    particlePipeline = std::make_unique<EnginePipeline>(device, "../Test/shaders/particle.vert.spv", "../Test/shaders/particle.frag.spv", particleConfig);

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

        auto animView = registry.view<AnimatorComponent>();
        for (auto entity : animView) {
            auto& animComp = animView.get<AnimatorComponent>(entity);
            if (animComp.animator) {
                // (선택) 여기서 특정 조건에 따라 playAnimation을 호출할 수도 있습니다.
                // 지금은 이미 생성자에서 idle을 세팅했으므로 업데이트만 해줍니다.
                animComp.animator->updateAnimation(frameTime); 
            }
        }

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

        float waterHeight = 0.5f;
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

        auto frustumPlanes = camera.getFrustumPlanes();
        
        // CullingComponent와 BoundingSphere, Transform을 가진 모든 엔티티 뷰 추출
        auto cullView = registry.view<CullingComponent, BoundingSphereComponent, TransformComponent>();
        
        // 엔티티들을 배열로 모아서 청크(Chunk) 단위로 나눕니다.
        std::vector<entt::entity> entities(cullView.begin(), cullView.end());
        size_t chunkSize = 100; // 한 스레드가 처리할 오브젝트 개수
        
        for (size_t i = 0; i < entities.size(); i += chunkSize) {
            size_t end = std::min(i + chunkSize, entities.size());
            
            // ★ 스레드 풀에 컬링 작업을 던집니다! (비동기 병렬 처리)
            threadPool->enqueue([&, i, end]() {
                for (size_t j = i; j < end; ++j) {
                    auto entity = entities[j];
                    auto& transform = cullView.get<TransformComponent>(entity);
                    auto& sphere = cullView.get<BoundingSphereComponent>(entity);
                    auto& cull = cullView.get<CullingComponent>(entity);

                    // 월드 좌표 적용된 중심점과 반지름 계산
                    glm::vec3 center = transform.translation + sphere.offset;
                    // 스케일 중 가장 큰 값을 곱해줍니다
                    float maxScale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
                    float radius = sphere.radius * maxScale;

                    cull.isVisible = true;

                    // 6개의 평면 중 하나라도 구체가 완전히 뒤쪽에 있으면 컬링(제외)!
                    for (const auto& plane : frustumPlanes) {
                        float distance = glm::dot(plane.normal, center) + plane.distance;
                        if (distance < -radius) {
                            cull.isVisible = false; // 안 보임!
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
        // [3] 대망의 렌더링 시작! (복잡한 펜스, 이미지 획득이 증발했습니다!)
        // =======================================================
        if (auto commandBuffer = engineRenderer->beginFrame()) {
            //particle
            // ★ [추가 1] 무대 세팅 전, 공장(Compute) 먼저 가동!
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->getPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
            vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &frameTime);

            uint32_t groupCountX = (PARTICLE_COUNT + 255) / 256;
            vkCmdDispatch(commandBuffer, groupCountX, 1, 1);

            // ★ [추가 2] GPU야, 파티클 계산 다 끝날 때까지 화면 그리지 말고 기다려!
            VkBufferMemoryBarrier particleBarrier{};
            particleBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            particleBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            particleBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            particleBarrier.buffer = particleSSBO->getBuffer();
            particleBarrier.offset = 0; particleBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                0, 0, nullptr, 1, &particleBarrier, 0, nullptr
            );
            
            // --- 패스 1: 그림자 렌더링 (Shadow) ---
            VkRenderPassBeginInfo shadowPassInfo{};
            shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            shadowPassInfo.renderPass = engineShadow->getRenderPass();
            shadowPassInfo.framebuffer = engineShadow->getFramebuffer();
            shadowPassInfo.renderArea.extent = {engineShadow->getWidth(), engineShadow->getHeight()};
            VkClearValue depthClear{}; depthClear.depthStencil = {1.0f, 0};
            shadowPassInfo.clearValueCount = 1; shadowPassInfo.pClearValues = &depthClear;

            vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline->getPipeline());
            
            auto modelView = registry.view<TransformComponent, ModelComponent>();
            for (auto entity : modelView) {
                auto &transform = modelView.get<TransformComponent>(entity);
                auto &modelComp = modelView.get<ModelComponent>(entity);
                SimplePushConstantData push{}; push.modelMatrix = transform.mat4();
                vkCmdPushConstants(commandBuffer, shadowPipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline->getPipelineLayout(), 0, 1, &modelComp.mainSet, 0, nullptr);
                modelComp.model->bind(commandBuffer); modelComp.model->draw(commandBuffer);
            }
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
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline->getPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline->getPipelineLayout(), 0, 1, &computeDescriptorSet, 0, nullptr);
            vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);
            
            // 3. 물
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipeline->getPipeline());
            SimplePushConstantData waterPush{}; 
            waterPush.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, waterHeight, 0.0f));
            vkCmdPushConstants(commandBuffer, waterPipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &waterPush);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipeline->getPipelineLayout(), 0, 1, &waterSet, 0, nullptr);
            
            auto floorModel = assetManager->getModel("FloorModel");
            floorModel->bind(commandBuffer);
            floorModel->draw(commandBuffer);

            // ★ 메인 무대 닫기!
            engineRenderer->endSwapChainRenderPass(commandBuffer);

            // =======================================================
            // [4] 화면 출력 제출 (이 한 줄이 QueueSubmit, Present 등을 다 해줍니다)
            // =======================================================
            engineRenderer->endFrame();
        }
    }
}

// 2. 엔티티 스폰 API 구현
void GameApp::spawnPlayer(const std::string& modelName, glm::vec3 position) {
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    
    auto &transform = registry.emplace<TransformComponent>(player);
    transform.translation = position;
    transform.scale = {0.01f, 0.01f, 0.01f}; // 모델에 따라 기본 스케일은 하드코딩하거나 매개변수로 뺄 수 있습니다.
    
    // ★ 창고(models)에서 이름으로 모델을 찾아서 넣어줍니다!
    auto &modelComp = registry.emplace<ModelComponent>(player, assetManager->getModel(modelName));
    modelComp.roughness = 0.9f; 
    
    uint32_t ragdollID = physicsEngine.createSimpleRagdoll(position);
    registry.emplace<RagdollComponent>(player, ragdollID);

    registry.emplace<BoundingSphereComponent>(player, 100.0f); // 예: 반경 100
    registry.emplace<CullingComponent>(player);
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
            glm::vec3 pos{0.0f}, scale{1.0f};
            if (entityData.contains("transform")) {
                auto& t = entityData["transform"];
                pos = glm::vec3(t["position"][0], t["position"][1], t["position"][2]);
                scale = glm::vec3(t["scale"][0], t["scale"][1], t["scale"][2]);
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

                    float radius = modelComp.model->getBoundingRadius();
                    glm::vec3 center = modelComp.model->getBoundingCenter();

                    registry.emplace<BoundingSphereComponent>(entity, radius, center); 
                    registry.emplace<CullingComponent>(entity);
                } catch (const std::exception& e) {
                    std::cerr << "경고: " << e.what() << std::endl;
                }
            }

            // 3. 태그에 따른 특수 컴포넌트 장착
            if (tag == "Player") {
                registry.emplace<PlayerTag>(entity);
                uint32_t ragdollID = physicsEngine.createSimpleRagdoll(pos);
                registry.emplace<RagdollComponent>(entity, ragdollID);
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
        }
    }



    // ★ 수정 4: 모든 에셋과 ECS 엔티티 세팅이 끝났으므로, 디스크립터를 조립합니다!
    setupDescriptorsAndPipelines();
    
    // (선택) 조명이나 카메라도 JSON에서 읽어오도록 확장할 수 있습니다.
    std::cout << "씬 로딩 완료: " << j["scene_name"] << std::endl;
}