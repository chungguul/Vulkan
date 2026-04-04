#include "EngineAnimator.hpp"

EngineAnimator::EngineAnimator(EngineAnimation* animation) {
    currentTime = 0.0f;
    currentAnimation = animation;
    
    // 뼈대 행렬 100개 공간 미리 확보 (기본값: 단위 행렬)
    finalBoneMatrices.reserve(100);
    for (int i = 0; i < 100; i++) {
        finalBoneMatrices.push_back(glm::mat4(1.0f));
    }
}

void EngineAnimator::updateAnimation(float dt) {
    deltaTime = dt;
    if (currentAnimation) {
        // 1. 애니메이션 속도에 맞춰 현재 시간 증가
        currentTime += currentAnimation->getTicksPerSecond() * dt;
        
        // 2. 애니메이션이 끝까지 가면 처음으로 루프(Loop)
        currentTime = fmod(currentTime, currentAnimation->getDuration());
        
        // 3. 최상위 뼈대(Root)부터 변환 계산 시작
        calculateBoneTransform(&currentAnimation->getRootNode(), glm::mat4(1.0f));
    }
}

void EngineAnimator::playAnimation(EngineAnimation* pAnimation) {
    currentAnimation = pAnimation;
    currentTime = 0.0f;
}

void EngineAnimator::calculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform) {
    std::string nodeName = node->name;
    glm::mat4 nodeTransform = node->transformation;

    // 현재 노드(뼈)에 해당하는 애니메이션 키프레임이 있는지 확인
    EngineBone* bone = currentAnimation->findBone(nodeName);
    if (bone) {
        // 현재 시간에 맞춰 위치/회전/크기 보간
        bone->update(currentTime);
        nodeTransform = bone->getLocalTransform();
    }

    // 부모의 변환 행렬에 나의 변환 행렬을 곱함 (어깨가 돌면 팔꿈치도 따라 돌게 됨)
    glm::mat4 globalTransformation = parentTransform * nodeTransform;

    auto boneInfoMap = currentAnimation->getBoneInfoMap();
    if (boneInfoMap.find(nodeName) != boneInfoMap.end()) {
        int index = boneInfoMap[nodeName].id;
        glm::mat4 offset = boneInfoMap[nodeName].offset;
        
        // 셰이더로 보낼 최종 행렬 = 원점으로 가져오는 행렬(Offset) * 애니메이션 변환 행렬
        finalBoneMatrices[index] = globalTransformation * offset;
    }

    // 내 모든 자식 뼈대들에게도 계산을 지시
    for (int i = 0; i < node->childrenCount; i++) {
        calculateBoneTransform(&node->children[i], globalTransformation);
    }
}