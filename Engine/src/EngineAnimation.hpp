#pragma once

#include "EngineModel.hpp"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <assimp/scene.h>
#include <vector>
#include <string>
#include <map>

// 키프레임 구조체들
struct KeyPosition { glm::vec3 position; float timeStamp; };
struct KeyRotation { glm::quat orientation; float timeStamp; };
struct KeyScale { glm::vec3 scale; float timeStamp; };

// 개별 뼈대 하나하나의 움직임을 담당 (위치, 회전, 크기 보간 연산)
class EngineBone {
public:
    EngineBone(const std::string& name, int ID, const aiNodeAnim* channel);
    void update(float animationTime);
    glm::mat4 getLocalTransform() const { return localTransform; }
    std::string getBoneName() const { return name; }
    int getBoneID() const { return id; }

private:
    int getPositionIndex(float animationTime);
    int getRotationIndex(float animationTime);
    int getScaleIndex(float animationTime);
    float getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime);
    
    glm::mat4 interpolatePosition(float animationTime);
    glm::mat4 interpolateRotation(float animationTime);
    glm::mat4 interpolateScale(float animationTime);

    std::vector<KeyPosition> positions;
    std::vector<KeyRotation> rotations;
    std::vector<KeyScale> scales;
    
    glm::mat4 localTransform{1.0f};
    std::string name;
    int id;
};

// 뼈대들의 부모-자식 관계도 (예: 팔 -> 손목 -> 손가락)
struct AssimpNodeData {
    glm::mat4 transformation;
    std::string name;
    int childrenCount;
    std::vector<AssimpNodeData> children;
};

// FBX에서 애니메이션 트랙 전체를 읽어오는 관리자
class EngineAnimation {
public:
    EngineAnimation(const std::string& animationPath, EngineModel* model);
    
    float getTicksPerSecond() const { return ticksPerSecond; }
    float getDuration() const { return duration; }
    const AssimpNodeData& getRootNode() const { return rootNode; }
    const std::map<std::string, BoneInfo>& getBoneInfoMap() const { return boneInfoMap; }
    EngineBone* findBone(const std::string& name);

private:
    void readMissingBones(const aiAnimation* animation, EngineModel& model);
    void readHierarchyData(AssimpNodeData& dest, const aiNode* src);

    float duration;
    float ticksPerSecond;
    std::vector<EngineBone> bones;
    AssimpNodeData rootNode;
    std::map<std::string, BoneInfo> boneInfoMap;
};