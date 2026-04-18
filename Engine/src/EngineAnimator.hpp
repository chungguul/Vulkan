#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "EngineAnimation.hpp"
#include "EngineModel.hpp"

class EngineAnimator {
public:
    EngineAnimator(EngineAnimation* animation);
    
    void updateAnimation(float dt);
    
    void playAnimation(EngineAnimation* pAnimation);
    
    std::vector<glm::mat4> getFinalBoneMatrices() const { return finalBoneMatrices; }

private:
    void calculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

    std::vector<glm::mat4> finalBoneMatrices;
    EngineAnimation* currentAnimation;
    float currentTime;
    float deltaTime;
};