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
#include "AssetManager.hpp"
#include "EngineParticleSystem.hpp"
#include "EngineShadowSystem.hpp"
#include "EngineWaterSystem.hpp"

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

struct GlobalUbo {
    glm::mat4 projectionView;
    glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f};
    glm::vec3 lightDirection = glm::normalize(glm::vec3(0.5f, -3.0f, 1.0f));
    alignas(16) glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};

    //glm::mat4 finalBonesMatrices[MAX_BONES];

    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 clipPlane;
    float time;

    alignas(16) PointLight pointLights[MAX_POINT_LIGHTS];
    int numPointLights;
};


class GameApp {
public:
    static constexpr int WIDTH = 1920;
    static constexpr int HEIGHT = 1080;

    GameApp();
    ~GameApp();

    GameApp(const GameApp &) = delete;
    GameApp &operator=(const GameApp &) = delete;

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
    std::unique_ptr<EngineShadowSystem> shadowSystem;
    std::unique_ptr<EngineWaterSystem> waterRenderSystem;

    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout waterSetLayout;
    VkDescriptorSet waterSet;

    // --- 3. 에셋 및 버퍼 ---
    std::unique_ptr<EngineCubemap> skyboxCubemap;
    std::unique_ptr<EngineBuffer> uboBufferMain;
    std::unique_ptr<EngineBuffer> uboBufferReflection;
    std::unique_ptr<EngineBuffer> uboBufferRefraction;

    std::unique_ptr<AssetManager> assetManager;

    std::unique_ptr<EngineBuffer> boneSSBO;

    // --- 4. 게임 로직 시스템 ---
    entt::registry registry;
    EnginePhysics physicsEngine;
    EngineCamera camera;
    KeyboardMovementController cameraController;

    std::unique_ptr<EngineRenderer> engineRenderer;

    std::unique_ptr<EngineParticleSystem> particleSystem;

    std::unique_ptr<EngineThreadPool> threadPool;
};

