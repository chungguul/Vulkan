#include "EnginePhysics.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/Skeleton/SkeletonPose.h>

#include <iostream>
#include <cstdarg>
#include <thread>

using namespace JPH;

// --- 콜백 함수: Jolt 내부에서 에러가 났을 때 터미널에 출력해줍니다 ---
static void TraceImpl(const char *inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    std::cout << "[Jolt Physics] " << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint32_t inLine) {
    std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << std::endl;
    return true; // true를 반환하면 브레이크포인트가 걸립니다.
}
#endif

// --- 물리 레이어 정의 (사물을 고정된 것과 움직이는 것으로 분류) ---
namespace Layers {
    static constexpr ObjectLayer NON_MOVING = 0; // 바닥, 벽 등
    static constexpr ObjectLayer MOVING = 1;     // 캐릭터, 래그돌 등
    static constexpr ObjectLayer NUM_LAYERS = 2;
};

namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS(2);
};

// --- Jolt 충돌 필터 구현체 ---
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }
    virtual uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }
private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING: return true;
            default: return false;
        }
    }
};

class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING: return inObject2 == Layers::MOVING; // 움직이는 것과만 충돌
            case Layers::MOVING: return true; // 다 충돌함
            default: return false;
        }
    }
};

// --- EnginePhysics 멤버 함수 구현 ---

EnginePhysics::EnginePhysics() {}

EnginePhysics::~EnginePhysics() {
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    
    delete (BPLayerInterfaceImpl*)bpLayerInterface;
    delete (ObjectVsBroadPhaseLayerFilterImpl*)objVsBpLayerFilter;
    delete (ObjectLayerPairFilterImpl*)objVsObjLayerFilter;
}

void EnginePhysics::init() {
    RegisterDefaultAllocator();

    Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    AssertFailed = AssertFailedImpl;
#endif

    Factory::sInstance = new Factory();
    RegisterTypes();

    // 임시 메모리 할당 (충돌 계산 시 사용, 10MB)
    tempAllocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);
    
    // 멀티스레딩 세팅 (CPU 코어 개수에 맞춰서 스레드 생성)
    jobSystem = std::make_unique<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    bpLayerInterface = new BPLayerInterfaceImpl();
    objVsBpLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    objVsObjLayerFilter = new ObjectLayerPairFilterImpl();

    physicsSystem = std::make_unique<PhysicsSystem>();
    
    // 물리 세상의 최대 한계 설정 (Max Bodies, Num Mutexes, Max Body Pairs, Max Contact Constraints)
    physicsSystem->Init(1024, 0, 1024, 1024, 
                        *(BPLayerInterfaceImpl*)bpLayerInterface, 
                        *(ObjectVsBroadPhaseLayerFilterImpl*)objVsBpLayerFilter, 
                        *(ObjectLayerPairFilterImpl*)objVsObjLayerFilter);
                        
    std::cout << "성공: Jolt Physics 시스템 초기화 완료!" << std::endl;
}

void EnginePhysics::update(float deltaTime) {
    // 프레임당 시뮬레이션 계산 (보통 1 스텝 처리)
    const int cCollisionSteps = 1;
    physicsSystem->Update(deltaTime, cCollisionSteps, tempAllocator.get(), jobSystem.get());
}

void EnginePhysics::createFloor() {
    // 100x1x100 크기의 거대한 박스를 만들어 바닥으로 씁니다.
    JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    JPH::ShapeRefC floorShape = floorShapeSettings.Create().Get();
    
    // 바닥이므로 Static(고정), NON_MOVING 레이어 사용
    JPH::BodyCreationSettings floorSettings(floorShape, JPH::RVec3(0.0f, -1.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NON_MOVING);
    
    physicsSystem->GetBodyInterface().CreateAndAddBody(floorSettings, JPH::EActivation::DontActivate);
}

uint32_t EnginePhysics::createBox(glm::vec3 position, glm::vec3 halfExtents, bool isDynamic) {
    JPH::BoxShapeSettings boxShapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    JPH::ShapeRefC boxShape = boxShapeSettings.Create().Get();
    
    // isDynamic이 true면 움직이는 물체(중력 영향 받음), false면 고정된 물체
    JPH::BodyCreationSettings boxSettings(boxShape, 
                                          JPH::RVec3(position.x, position.y, position.z), 
                                          JPH::Quat::sIdentity(), 
                                          isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static, 
                                          isDynamic ? Layers::MOVING : Layers::NON_MOVING);
    
    // 물체를 생성하고, 바로 활성화(떨어지기 시작) 시킵니다.
    JPH::BodyID bodyID = physicsSystem->GetBodyInterface().CreateAndAddBody(boxSettings, JPH::EActivation::Activate);
    
    // Jolt의 BodyID를 단순한 unsigned int로 변환하여 반환 (완벽한 캡슐화!)
    return bodyID.GetIndexAndSequenceNumber();
}

glm::vec3 EnginePhysics::getBodyPosition(uint32_t id) {
    JPH::BodyID bodyID(id);
    JPH::RVec3 pos = physicsSystem->GetBodyInterface().GetPosition(bodyID);
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

uint32_t EnginePhysics::createSimpleRagdoll(glm::vec3 position) {
    using namespace JPH;

    Ref<Skeleton> skeleton = new Skeleton();
    // 뼈대 계층 구조 정의 (인덱스가 중요합니다!)
    skeleton->AddJoint("Pelvis", -1);  // 0번: 루트(골반)
    skeleton->AddJoint("Head", 0);     // 1번: 머리 (부모: 0)
    skeleton->AddJoint("L_Thigh", 0);  // 2번: 왼 허벅지 (부모: 0)
    skeleton->AddJoint("L_Calf", 2);   // 3번: 왼 종아리 (부모: 2)
    skeleton->AddJoint("R_Thigh", 0);  // 4번: 오른 허벅지 (부모: 0)
    skeleton->AddJoint("R_Calf", 4);   // 5번: 오른 종아리 (부모: 4)
    skeleton->AddJoint("L_UpperArm", 0);  // 6번: 왼 위팔 (어깨는 몸통에 연결)
    skeleton->AddJoint("L_Forearm", 6);   // 7번: 왼 아래팔
    skeleton->AddJoint("R_UpperArm", 0);  // 8번: 오른 위팔
    skeleton->AddJoint("R_Forearm", 8);   // 9번: 오른 아래팔

    Ref<RagdollSettings> ragdollSettings = new RagdollSettings();
    ragdollSettings->mSkeleton = skeleton;

    // --- 1. 파트(캡슐) 생성 ---
auto createPart = [](float length, float radius, RVec3 pos) {
        RagdollSettings::Part part;
        part.mPosition = pos;
        part.mRotation = Quat::sIdentity();
        
        ShapeRefC shape = CapsuleShapeSettings(length, radius).Create().Get();
        part.SetShape(shape);
        
        part.mMotionType = EMotionType::Dynamic;
        part.mObjectLayer = Layers::MOVING;
        return part;
    };

    // 0: Pelvis
    ragdollSettings->mParts.push_back(createPart(0.15f, 0.15f, RVec3(0, 0.9f, 0)));
    // 1: Head
    ragdollSettings->mParts.push_back(createPart(0.1f, 0.12f, RVec3(0, 1.3f, 0)));
    // 2: L_Thigh, 3: L_Calf
    ragdollSettings->mParts.push_back(createPart(0.2f, 0.08f, RVec3(-0.15f, 0.6f, 0)));
    ragdollSettings->mParts.push_back(createPart(0.2f, 0.07f, RVec3(-0.15f, 0.2f, 0)));
    // 4: R_Thigh, 5: R_Calf
    ragdollSettings->mParts.push_back(createPart(0.2f, 0.08f, RVec3(0.15f, 0.6f, 0)));
    ragdollSettings->mParts.push_back(createPart(0.2f, 0.07f, RVec3(0.15f, 0.2f, 0)));
    // 6: L_UpperArm, 7: L_Forearm
    ragdollSettings->mParts.push_back(createPart(0.15f, 0.05f, JPH::RVec3(-0.25f, 1.0f, 0)));
    ragdollSettings->mParts.push_back(createPart(0.15f, 0.04f, JPH::RVec3(-0.25f, 0.7f, 0)));
    // 8: R_UpperArm, 9: R_Forearm
    ragdollSettings->mParts.push_back(createPart(0.15f, 0.05f, JPH::RVec3(0.25f, 1.0f, 0)));
    ragdollSettings->mParts.push_back(createPart(0.15f, 0.04f, JPH::RVec3(0.25f, 0.7f, 0)));

    // --- 2. 관절(Constraint) 연결 ---
    
    // (1) 목 관절 (SwingTwist)
    SwingTwistConstraintSettings* neck = new SwingTwistConstraintSettings();
    neck->mPosition1 = neck->mPosition2 = RVec3(0, 1.1f, 0);
    neck->mTwistAxis1 = neck->mTwistAxis2 = Vec3::sAxisY();
    neck->mPlaneAxis1 = neck->mPlaneAxis2 = Vec3::sAxisX();
    neck->mNormalHalfConeAngle = JPH_PI / 6.0f;
    neck->mPlaneHalfConeAngle = JPH_PI / 6.0f;
    ragdollSettings->mParts[1].mToParent = neck;

    // (2) 골반-허벅지 관절 (다리가 찢어지지 않게 제한된 코깔콘 모양 SwingTwist)
    auto createHipConstraint = [](RVec3 pos) {
        SwingTwistConstraintSettings* hip = new SwingTwistConstraintSettings();
        hip->mPosition1 = hip->mPosition2 = pos;
        hip->mTwistAxis1 = hip->mTwistAxis2 = Vec3::sAxisY(); // 아래를 향함
        hip->mPlaneAxis1 = hip->mPlaneAxis2 = Vec3::sAxisX();
        hip->mNormalHalfConeAngle = JPH_PI / 4.0f; // 다리 벌림 제한
        hip->mPlaneHalfConeAngle = JPH_PI / 4.0f;  // 다리 앞뒤 제한
        return hip;
    };
    ragdollSettings->mParts[2].mToParent = createHipConstraint(RVec3(-0.15f, 0.8f, 0)); // 왼 고관절
    ragdollSettings->mParts[4].mToParent = createHipConstraint(RVec3(0.15f, 0.8f, 0));  // 오른 고관절

    // (3) 무릎 관절 (오직 X축으로만 꺾이는 Hinge)
    auto createKneeConstraint = [](RVec3 pos) {
        HingeConstraintSettings* knee = new HingeConstraintSettings();
        knee->mPoint1 = knee->mPoint2 = pos;
        knee->mHingeAxis1 = knee->mHingeAxis2 = Vec3::sAxisX(); // 무릎이 접히는 축
        knee->mNormalAxis1 = knee->mNormalAxis2 = Vec3::sAxisZ();
        knee->mLimitsMin = 0.0f;           // 무릎이 반대로 꺾이지 않게
        knee->mLimitsMax = JPH_PI * 0.8f;  // 뒤로는 140도 정도 꺾임
        return knee;
    };
    ragdollSettings->mParts[3].mToParent = createKneeConstraint(RVec3(-0.15f, 0.4f, 0)); // 왼 무릎
    ragdollSettings->mParts[5].mToParent = createKneeConstraint(RVec3(0.15f, 0.4f, 0));  // 오른 무릎

    // 어깨 관절 (팔이 빙글빙글 돌 수 있게 SwingTwist)
    auto createShoulderConstraint = [](JPH::RVec3 pos) {
        JPH::SwingTwistConstraintSettings* shoulder = new JPH::SwingTwistConstraintSettings();
        shoulder->mPosition1 = shoulder->mPosition2 = pos;
        shoulder->mTwistAxis1 = shoulder->mTwistAxis2 = JPH::Vec3::sAxisX(); // 팔이 뻗는 방향
        shoulder->mPlaneAxis1 = shoulder->mPlaneAxis2 = JPH::Vec3::sAxisY();
        shoulder->mNormalHalfConeAngle = JPH_PI / 2.0f; 
        shoulder->mPlaneHalfConeAngle = JPH_PI / 2.0f;
        return shoulder;
    };
    ragdollSettings->mParts[6].mToParent = createShoulderConstraint(JPH::RVec3(-0.2f, 1.1f, 0)); // 왼 어깨
    ragdollSettings->mParts[8].mToParent = createShoulderConstraint(JPH::RVec3(0.2f, 1.1f, 0));  // 오른 어깨

    // 팔꿈치 관절 (무릎처럼 접히기만 하는 Hinge)
    auto createElbowConstraint = [](JPH::RVec3 pos) {
        JPH::HingeConstraintSettings* elbow = new JPH::HingeConstraintSettings();
        elbow->mPoint1 = elbow->mPoint2 = pos;
        elbow->mHingeAxis1 = elbow->mHingeAxis2 = JPH::Vec3::sAxisX(); // 접히는 축
        elbow->mNormalAxis1 = elbow->mNormalAxis2 = JPH::Vec3::sAxisZ();
        elbow->mLimitsMin = 0.0f;           
        elbow->mLimitsMax = JPH_PI * 0.8f;  // 팔이 안쪽으로만 140도 접힘
        return elbow;
    };
    ragdollSettings->mParts[7].mToParent = createElbowConstraint(JPH::RVec3(-0.25f, 0.85f, 0)); // 왼 팔꿈치
    ragdollSettings->mParts[9].mToParent = createElbowConstraint(JPH::RVec3(0.25f, 0.85f, 0));  // 오른 팔꿈치

    // --- 3. 래그돌 생성 (기존과 동일) ---
    Ragdoll* ragdoll = ragdollSettings->CreateRagdoll(0, 0, physicsSystem.get());
    
    SkeletonPose pose;
    pose.SetSkeleton(skeleton);
    pose.CalculateJointMatrices(); // 기본 차렷 자세의 행렬 계산
    
    // [해결 2] Jolt 5.5.0 정석: SetPose에 시작 위치와 행렬 데이터를 한 번에 전달
    ragdoll->SetPose(RVec3(position.x, position.y, position.z), pose.GetJointMatrices().data());
    
    ragdoll->AddToPhysicsSystem(EActivation::Activate);
    
    ragdolls.push_back(ragdoll);
    return (uint32_t)(ragdolls.size() - 1);
}


void EnginePhysics::updateRagdollBones(uint32_t ragdollID, glm::mat4* outBones, int maxBones) {
    if (ragdollID >= ragdolls.size()) return;
    
    JPH::Ragdoll* ragdoll = ragdolls[ragdollID];
    const JPH::BodyInterface& bi = physicsSystem->GetBodyInterface();

    int bodyCount = (int)ragdoll->GetBodyCount();
    
    for (int i = 0; i < bodyCount && i < maxBones; ++i) {
        BodyID bid = ragdoll->GetBodyID(i);
        RMat44 jointMat = bi.GetWorldTransform(bid);
        
        //std:: cout << jointMat << std::endl;

        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                outBones[i][col][row] = jointMat(row, col);
            }
        }
    }
}

void EnginePhysics::applyImpulseToRagdoll(uint32_t ragdollID, glm::vec3 impulse, int partIndex) {
    if (ragdollID >= ragdolls.size()) return;
    
    // 특정 파트(기본값 0: Pelvis/몸통)의 BodyID를 가져와서 힘을 가합니다.
    JPH::BodyID bodyID = ragdolls[ragdollID]->GetBodyID(partIndex);
    physicsSystem->GetBodyInterface().AddImpulse(bodyID, JPH::Vec3(impulse.x, impulse.y, impulse.z));
}