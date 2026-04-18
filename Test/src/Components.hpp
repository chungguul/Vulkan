#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "EngineModel.hpp"
#include "EngineAnimation.hpp"
#include "EngineAnimator.hpp"

#include <memory>
#include <cstdint>

// 엔티티 태그
struct PlayerTag {};
struct FloorTag {};
struct CameraTag {};
struct PropTag {};

struct TransformComponent {
    glm::vec3 translation{};
    glm::vec3 scale{1.f, 1.f, 1.f};
    glm::vec3 rotation{};

    // 행렬 변환
    glm::mat4 mat4() const {
        auto transform = glm::translate(glm::mat4{1.f}, translation);
        transform = glm::rotate(transform, rotation.y, {0.f, 1.f, 0.f});
        transform = glm::rotate(transform, rotation.x, {1.f, 0.f, 0.f});
        transform = glm::rotate(transform, rotation.z, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, scale);
        return transform;
    }
};

//렌더링할 3D 모델
struct ModelComponent {
    std::shared_ptr<EngineModel> model;

    float roughness{0.8f};
    float metallic{0.0f};

    std::vector<VkDescriptorSet> mainSets;
    std::vector<VkDescriptorSet> reflectionSets;
    std::vector<VkDescriptorSet> refractionSets;
};




//RigidBody
struct RigidBodyComponent {
    uint32_t bodyID; 
};

//래그돌
struct RagdollComponent {
    uint32_t ragdollID;
};

struct PointLightComponent {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{10.0f};
};

struct BoundingSphereComponent {
    float radius = 1.0f;
    glm::vec3 offset = glm::vec3(0.0f);
};

// 컬링 결과 저장
struct CullingComponent {
    bool isVisible = true;
};

struct AnimatorComponent {
    std::shared_ptr<EngineAnimation> currentAnimation;
    std::unique_ptr<EngineAnimator> animator;

    AnimatorComponent(std::shared_ptr<EngineAnimation> anim) {
        currentAnimation = anim;
        animator = std::make_unique<EngineAnimator>(currentAnimation.get());
    }
    int characterIndex = 0;
};

//액체 컴포넌트
struct WaterComponent {
    float height = 0.5f;       
    float waveSpeed = 0.05f;   
    
    //defalut DuDv, Nomarl map
    std::string dudvTexture = "WaterDUDV";
    std::string normalTexture = "WaterNormal";

    std::vector<VkDescriptorSet> waterSets;
};

struct MaterialComponent {
    std::string albedoTexture;
};

struct SimplePushConstantData {
    glm::mat4 modelMatrix{1.0f};
    float roughness{0.8f};
    float metallic{0.0f};
    int characterIndex{0};
};