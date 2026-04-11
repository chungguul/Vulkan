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

//위치, 회전, 크기 정보를 담는 부품
struct TransformComponent {
    glm::vec3 translation{};
    glm::vec3 scale{1.f, 1.f, 1.f};
    glm::vec3 rotation{};

    // 행렬 변환 최적화 (기존 EngineGameObject::TransformComponent와 동일)
    glm::mat4 mat4() const {
        auto transform = glm::translate(glm::mat4{1.f}, translation);
        transform = glm::rotate(transform, rotation.y, {0.f, 1.f, 0.f});
        transform = glm::rotate(transform, rotation.x, {1.f, 0.f, 0.f});
        transform = glm::rotate(transform, rotation.z, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, scale);
        return transform;
    }
};

//렌더링할 3D 모델 정보를 담는 부품
struct ModelComponent {
    std::shared_ptr<EngineModel> model;

    float roughness{0.8f};
    float metallic{0.0f};

    std::vector<VkDescriptorSet> mainSets;
    std::vector<VkDescriptorSet> reflectionSets;
    std::vector<VkDescriptorSet> refractionSets;
};

//(나중에 추가될 부품들: PhysicsComponent, WaterComponent 등...)



//물리 엔진의 강체(RigidBody) 정보를 담는 부품
struct RigidBodyComponent {
    // Jolt Physics 세계에서 이 오브젝트를 식별하는 고유 ID를 저장합니다.
    uint32_t bodyID; 
};

//래그돌 정보 담기
struct RagdollComponent {
    uint32_t ragdollID; // Jolt 내부의 래그돌 배열 인덱스
};

struct PointLightComponent {
    glm::vec3 color{1.0f, 1.0f, 1.0f}; // 빛의 색상
    float intensity{10.0f};            // 빛의 밝기 (PBR에서는 수백~수천 단위도 씁니다)
};

// 모델의 크기를 감싸는 가상의 구체
struct BoundingSphereComponent {
    float radius = 1.0f; // 기본 반지름
    glm::vec3 offset = glm::vec3(0.0f); // 모델 중심점 보정
};

// 컬링 결과 저장 (매 프레임 스레드들이 이 값을 true/false로 바꿉니다)
struct CullingComponent {
    bool isVisible = true;
};

struct AnimatorComponent {
    std::shared_ptr<EngineAnimation> currentAnimation; // 현재 재생 중인 모션 (공유 가능)
    std::unique_ptr<EngineAnimator> animator;          // 현재 재생 시간, 프레임 (독립적)

    // 편의용 생성자
    AnimatorComponent(std::shared_ptr<EngineAnimation> anim) {
        currentAnimation = anim;
        animator = std::make_unique<EngineAnimator>(currentAnimation.get());
    }
    int characterIndex = 0;
};

//액체 컴포넌트 (이 명찰이 붙어있는 바닥은 알아서 찰랑이는 물로 렌더링 됨)
struct WaterComponent {
    float height = 0.5f;       
    float waveSpeed = 0.05f;   
    
    // ★ 액체마다 다른 텍스처를 쓸 수 있도록 이름 저장!
    std::string dudvTexture = "WaterDUDV";
    std::string normalTexture = "WaterNormal";

    // ★ 글로벌하게 쓰던 waterSet을 이제 액체 컴포넌트가 각자 가집니다!
    std::vector<VkDescriptorSet> waterSets;
};

// 렌더링 시 사용할 텍스처 이름(머티리얼)을 담는 부품
struct MaterialComponent {
    std::string albedoTexture; // 기본 색상 텍스처 이름 (예: "KoroneMap", "Wood")
    // 나중에 normalTexture, emissionTexture 등으로 무한히 확장 가능합니다!
};

struct SimplePushConstantData {
    glm::mat4 modelMatrix{1.0f};
    float roughness{0.8f};
    float metallic{0.0f};
    int characterIndex{0};
};