#pragma once
#include <glm/glm.hpp>

// Jolt Physics 기본 헤더
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>

#include <memory>
#include <vector>

class EnginePhysics {
public:
    EnginePhysics();
    ~EnginePhysics();

    // 복사 방지 (물리 엔진은 세상에 단 하나만 존재해야 합니다)
    EnginePhysics(const EnginePhysics&) = delete;
    EnginePhysics& operator=(const EnginePhysics&) = delete;

    void init();
    void update(float deltaTime);

    void createFloor();
    uint32_t createBox(glm::vec3 position, glm::vec3 halfExtents, bool isDynamic);
    glm::vec3 getBodyPosition(uint32_t bodyID);

    //PhysicsSystem 포인터가 반환
    JPH::PhysicsSystem* getPhysicsSystem() const { return physicsSystem.get(); }

    // 래그돌을 생성하고 ID를 반환합니다.
    uint32_t createSimpleRagdoll(glm::vec3 position);
    // 래그돌의 현재 물리 상태를 기반으로, 뼈대 행렬 배열을 덮어씌웁니다.
    void updateRagdollBones(uint32_t ragdollID, glm::mat4* outBones, int maxBones);

    void applyImpulseToRagdoll(uint32_t ragdollID, glm::vec3 impulse, int partIndex = 0);
private:
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;
    std::vector<JPH::Ref<JPH::Ragdoll>> ragdolls;

    // 충돌 필터링(레이어)을 위한 Jolt 인터페이스 클래스들의 포인터
    void* bpLayerInterface = nullptr;
    void* objVsBpLayerFilter = nullptr;
    void* objVsObjLayerFilter = nullptr;
};