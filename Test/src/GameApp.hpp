#pragma once

#include "EngineWindow.hpp"
#include "EngineDevice.hpp"
#include "EngineSwapChain.hpp"
#include "EnginePhysics.hpp"
#include "EngineCamera.hpp"
#include "KeyboardMovementController.hpp"

#include "EngineDescriptorManager.hpp"
#include "EngineWater.hpp"
#include "EngineShadow.hpp"
#include "EngineSkybox.hpp"
#include "EngineCubemap.hpp"
#include "EngineTexture.hpp"
#include "EngineModel.hpp"
#include "EngineBuffer.hpp"
#include "EngineAnimation.hpp"
#include "EngineAnimator.hpp"
#include "SimpleRenderSystem.hpp"
#include "Components.hpp"
#include "EngineThreadPool.hpp"

#include "EngineRenderer.hpp"

#include <nlohmann/json.hpp>

#include <entt/entt.hpp>
#include <memory>
#include <vector>

const int MAX_BONES = 100;
const int MAX_POINT_LIGHTS = 10;

struct PointLight {
    alignas(16) glm::vec4 position;
    alignas(16) glm::vec4 color;
};

struct Particle {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 velocity;
    alignas(16) glm::vec4 color;
};

struct GlobalUbo {
    glm::mat4 projectionView;
    glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f};
    glm::vec3 lightDirection = glm::normalize(glm::vec3(0.5f, -3.0f, 1.0f));
    alignas(16) glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};

    glm::mat4 finalBonesMatrices[MAX_BONES];

    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 clipPlane;
    float time;

    alignas(16) PointLight pointLights[MAX_POINT_LIGHTS];
    int numPointLights;
};

// 푸시 상수는 SimpleRenderSystem과 공유합니다.
struct SimplePushConstantData {
    glm::mat4 modelMatrix{1.0f};
    float roughness{0.8f};
    float metallic{0.0f};
};

class AssetManager {
    std::unordered_map<std::string, std::shared_ptr<EngineModel>> models;
    std::unordered_map<std::string, std::shared_ptr<EngineTexture>> textures;
public:
    void loadModel(const std::string& name, const std::string& path);
    auto getModel(const std::string& name);
};

class GameApp {
public:
    static constexpr int WIDTH = 1920;
    static constexpr int HEIGHT = 1080;

    GameApp();
    ~GameApp();

    GameApp(const GameApp &) = delete;
    GameApp &operator=(const GameApp &) = delete;

    //외부(main.cpp)에서 에셋을 등록할 수 있는 API
    void loadTexture(const std::string& name, const std::string& filepath);
    void loadModel(const std::string& name, const std::string& filepath);
    
    //외부에서 엔티티를 생성할 수 있는 API
    void spawnPlayer(const std::string& modelName, glm::vec3 position);
    void spawnFloor(glm::vec3 position);

    void loadSceneFromJSON(const std::string& filepath);

    void run();

private:
    void setupDescriptorsAndPipelines();

    // --- 1. 코어 시스템 ---
    EngineWindow window{WIDTH, HEIGHT, "Vulkan Engine"};
    EngineDevice device{window};

    // --- 2. 렌더링 서브시스템 ---
    std::unique_ptr<EngineDescriptorManager> descriptorManager;
    std::unique_ptr<EngineWater> engineWater;
    std::unique_ptr<EngineShadow> engineShadow;
    std::unique_ptr<EngineSkybox> engineSkybox;
    std::unique_ptr<SimpleRenderSystem> simpleRenderSystem;

    // 추가 파이프라인 (그림자, 물 전용)
    std::unique_ptr<EnginePipeline> shadowPipeline;
    std::unique_ptr<EnginePipeline> waterPipeline;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout waterSetLayout;
    VkDescriptorSet waterSet;

    // --- 3. 에셋 및 버퍼 ---
    std::unique_ptr<EngineCubemap> skyboxCubemap;
    std::unique_ptr<EngineBuffer> uboBufferMain;
    std::unique_ptr<EngineBuffer> uboBufferReflection;
    std::unique_ptr<EngineBuffer> uboBufferRefraction;

    //애니메이션 부분은 추후 하드코딩 없애는 방식으로 개선 예정
    std::unique_ptr<EngineAnimation> idleAnimation;
    std::unique_ptr<EngineAnimation> walkAnimation;
    std::unique_ptr<EngineAnimator> animator;

    std::unordered_map<std::string, std::shared_ptr<EngineTexture>> textures;
    std::unordered_map<std::string, std::shared_ptr<EngineModel>> models;

    // --- 4. 게임 로직 시스템 ---
    entt::registry registry;
    EnginePhysics physicsEngine;
    EngineCamera camera;
    KeyboardMovementController cameraController;

    std::unique_ptr<EngineRenderer> engineRenderer;


    //particle system
    static constexpr int PARTICLE_COUNT = 10000; // 1만 개!

    std::unique_ptr<EngineBuffer> particleSSBO;
    
    VkDescriptorSetLayout computeSetLayout;
    VkDescriptorSet computeDescriptorSet;
    VkPipelineLayout computePipelineLayout;
    
    std::unique_ptr<EnginePipeline> computePipeline;
    std::unique_ptr<EnginePipeline> particlePipeline;

    std::unique_ptr<EngineThreadPool> threadPool;
};

