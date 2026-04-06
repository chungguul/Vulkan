#include "EnginePhysics.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
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

    // 1. 뼈대(Skeleton) 정의
    Ref<Skeleton> skeleton = new Skeleton();
    skeleton->AddJoint("Body", -1); 
    skeleton->AddJoint("Head", 0);  

    // 2. 래그돌 세팅 생성
    Ref<RagdollSettings> ragdollSettings = new RagdollSettings();
    ragdollSettings->mSkeleton = skeleton;

    // 3. 각 뼈대 부위(Part) 설정
    // [해결 1] C++ 문법 함정 회피: 객체를 괄호나 지역 변수로 깔끔하게 생성해서 넘깁니다.
    
    // 몸통 (Index 0)
    RagdollSettings::Part torsoPart;
    torsoPart.mPosition = RVec3(0, 0.5f, 0);
    torsoPart.mRotation = Quat::sIdentity();
    // ShapeSettings 인스턴스화 후 Create().Get() 으로 ShapeRefC 추출
    ShapeRefC torsoShape = CapsuleShapeSettings(0.3f, 0.15f).Create().Get();
    torsoPart.SetShape(torsoShape); // mShape 직접 접근 금지, SetShape 사용!
    torsoPart.mMotionType = EMotionType::Dynamic;
    torsoPart.mObjectLayer = Layers::MOVING;
    ragdollSettings->mParts.push_back(torsoPart);

    // 머리 (Index 1)
    RagdollSettings::Part headPart;
    headPart.mPosition = RVec3(0, 1.0f, 0);
    headPart.mRotation = Quat::sIdentity();
    ShapeRefC headShape = CapsuleShapeSettings(0.1f, 0.15f).Create().Get();
    headPart.SetShape(headShape);
    headPart.mMotionType = EMotionType::Dynamic;
    headPart.mObjectLayer = Layers::MOVING;
    
    // 4. 목 관절(Constraint) 설정
    SwingTwistConstraintSettings* neckConstraint = new SwingTwistConstraintSettings();
    neckConstraint->mPosition1 = neckConstraint->mPosition2 = RVec3(0, 0.8f, 0);
    neckConstraint->mTwistAxis1 = neckConstraint->mTwistAxis2 = Vec3::sAxisY();
    neckConstraint->mPlaneAxis1 = neckConstraint->mPlaneAxis2 = Vec3::sAxisX();
    neckConstraint->mNormalHalfConeAngle = JPH_PI / 8.0f; // 매우 빡빡하게 제한
    neckConstraint->mPlaneHalfConeAngle = JPH_PI / 8.0f;
    neckConstraint->mTwistMinAngle = -JPH_PI / 12.0f; // 회전도 꽉 잡기
    neckConstraint->mTwistMaxAngle = JPH_PI / 12.0f;
    
    headPart.mToParent = neckConstraint; 
    ragdollSettings->mParts.push_back(headPart);

    // 5. 래그돌 생성 및 초기 위치 설정
    Ragdoll* ragdoll = ragdollSettings->CreateRagdoll(0, 0, physicsSystem.get());
    
    // [해결 2] 래그돌 월드 배치: 위치(RVec3)와 뼈대 행렬 배열을 직접 넘겨줍니다.
    SkeletonPose pose;
    pose.SetSkeleton(skeleton);
    pose.CalculateJointMatrices(); // 기본 차렷 자세의 행렬 계산
    
    // SetPose(시작 위치, 각 뼈대의 행렬 데이터) 형태로 호출
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