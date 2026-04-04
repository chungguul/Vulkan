#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "EngineAnimation.hpp"
#include "EngineModel.hpp"

class EngineAnimator {
public:
    EngineAnimator(EngineAnimation* animation);
    
    // 매 프레임(dt)마다 시간을 업데이트하고 뼈대 행렬을 계산합니다.
    void updateAnimation(float dt);
    
    // 다른 애니메이션으로 바꿀 때 사용합니다.
    void playAnimation(EngineAnimation* pAnimation);
    
    // 셰이더로 보낼 최종 뼈대 행렬들(최대 100개)을 반환합니다.
    std::vector<glm::mat4> getFinalBoneMatrices() const { return finalBoneMatrices; }

private:
    // 부모 뼈대부터 자식 뼈대까지 재귀적으로 타고 내려가며 변환을 누적하는 핵심 함수!
    void calculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

    std::vector<glm::mat4> finalBoneMatrices;
    EngineAnimation* currentAnimation;
    float currentTime;
    float deltaTime;
};