#pragma once
#include <glm/glm.hpp>

// Jolt Physics 기본 헤더
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#include <memory>

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

    // 나중에 바닥이나 래그돌을 만들 때 PhysicsSystem 포인터가 필요합니다.
    JPH::PhysicsSystem* getPhysicsSystem() const { return physicsSystem.get(); }

private:
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

    // 충돌 필터링(레이어)을 위한 Jolt 인터페이스 클래스들의 포인터
    void* bpLayerInterface = nullptr;
    void* objVsBpLayerFilter = nullptr;
    void* objVsObjLayerFilter = nullptr;
};