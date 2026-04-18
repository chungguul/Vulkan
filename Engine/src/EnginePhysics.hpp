#pragma once
#include <glm/glm.hpp>

#include "EngineModel.hpp"

// Jolt Physics 기본 헤더
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>

#include <memory>
#include <vector>
#include <map>

class EnginePhysics {
public:
    EnginePhysics();
    ~EnginePhysics();

    EnginePhysics(const EnginePhysics&) = delete;
    EnginePhysics& operator=(const EnginePhysics&) = delete;

    void init();
    void update(float deltaTime);

    void createFloor();
    uint32_t createBox(glm::vec3 position, glm::vec3 halfExtents, bool isDynamic);
    glm::vec3 getBodyPosition(uint32_t bodyID);

    JPH::PhysicsSystem* getPhysicsSystem() const { return physicsSystem.get(); }

    uint32_t createSimpleRagdoll(glm::vec3 position);
    void updateRagdollBones(uint32_t ragdollID, glm::mat4* outBones, int maxBones);
    void syncRagdollBones(uint32_t ragdollID, const std::map<std::string, BoneInfo>& boneInfoMap, glm::mat4* outFinalBones, glm::vec3& outRootPos, glm::vec3& outRootRot);

    void applyImpulseToRagdoll(uint32_t ragdollID, glm::vec3 impulse, int partIndex = 0);
private:
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;
    std::vector<JPH::Ref<JPH::Ragdoll>> ragdolls;

    void* bpLayerInterface = nullptr;
    void* objVsBpLayerFilter = nullptr;
    void* objVsObjLayerFilter = nullptr;
};