#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include "EngineModel.hpp"

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
};

//(나중에 추가될 부품들: PhysicsComponent, WaterComponent 등...)

#include <cstdint>

//물리 엔진의 강체(RigidBody) 정보를 담는 부품
struct RigidBodyComponent {
    // Jolt Physics 세계에서 이 오브젝트를 식별하는 고유 ID를 저장합니다.
    uint32_t bodyID; 
};

//래그돌 정보 담기
struct RagdollComponent {
    uint32_t ragdollID; // Jolt 내부의 래그돌 배열 인덱스
};