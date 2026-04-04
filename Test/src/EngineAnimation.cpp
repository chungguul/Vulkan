#include "EngineAnimation.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <cassert>
#include <algorithm>

// --- Assimp 행렬을 GLM 행렬로 변환하는 헬퍼 함수 ---
static glm::mat4 convertMatrixToGLMFormat(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

// ==============================================================================
// 1. EngineBone (뼈대 단일 객체) 구현
// ==============================================================================
EngineBone::EngineBone(const std::string& name, int ID, const aiNodeAnim* channel)
    : name(name), id(ID), localTransform(1.0f) {
    
    // 위치(Position) 키프레임 복사
    for (int i = 0; i < channel->mNumPositionKeys; ++i) {
        aiVector3D aiPosition = channel->mPositionKeys[i].mValue;
        positions.push_back({ {aiPosition.x, aiPosition.y, aiPosition.z}, (float)channel->mPositionKeys[i].mTime });
    }
    // 회전(Rotation) 키프레임 복사
    for (int i = 0; i < channel->mNumRotationKeys; ++i) {
        aiQuaternion aiOrientation = channel->mRotationKeys[i].mValue;
        rotations.push_back({ glm::quat(aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z), (float)channel->mRotationKeys[i].mTime });
    }
    // 크기(Scale) 키프레임 복사
    for (int i = 0; i < channel->mNumScalingKeys; ++i) {
        aiVector3D aiScale = channel->mScalingKeys[i].mValue;
        scales.push_back({ {aiScale.x, aiScale.y, aiScale.z}, (float)channel->mScalingKeys[i].mTime });
    }
}

// 현재 시간에 해당하는 키프레임 인덱스 찾기
int EngineBone::getPositionIndex(float animationTime) {
    for (int i = 0; i < positions.size() - 1; ++i) {
        if (animationTime < positions[i + 1].timeStamp) return i;
    }
    return 0;
}
int EngineBone::getRotationIndex(float animationTime) {
    for (int i = 0; i < rotations.size() - 1; ++i) {
        if (animationTime < rotations[i + 1].timeStamp) return i;
    }
    return 0;
}
int EngineBone::getScaleIndex(float animationTime) {
    for (int i = 0; i < scales.size() - 1; ++i) {
        if (animationTime < scales[i + 1].timeStamp) return i;
    }
    return 0;
}

// 두 키프레임 사이의 진행도(0.0 ~ 1.0) 계산
float EngineBone::getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) {
    float scaleFactor = 0.0f;
    float midWayLength = animationTime - lastTimeStamp;
    float framesDiff = nextTimeStamp - lastTimeStamp;
    scaleFactor = midWayLength / framesDiff;
    return scaleFactor;
}

// 보간(Interpolation) 연산
glm::mat4 EngineBone::interpolatePosition(float animationTime) {
    if (1 == positions.size()) return glm::translate(glm::mat4(1.0f), positions[0].position);
    int p0Index = getPositionIndex(animationTime);
    int p1Index = p0Index + 1;
    float scaleFactor = getScaleFactor(positions[p0Index].timeStamp, positions[p1Index].timeStamp, animationTime);
    glm::vec3 finalPosition = glm::mix(positions[p0Index].position, positions[p1Index].position, scaleFactor);
    return glm::translate(glm::mat4(1.0f), finalPosition);
}

glm::mat4 EngineBone::interpolateRotation(float animationTime) {
    if (1 == rotations.size()) return glm::toMat4(glm::normalize(rotations[0].orientation));
    int p0Index = getRotationIndex(animationTime);
    int p1Index = p0Index + 1;
    float scaleFactor = getScaleFactor(rotations[p0Index].timeStamp, rotations[p1Index].timeStamp, animationTime);
    glm::quat finalRotation = glm::slerp(rotations[p0Index].orientation, rotations[p1Index].orientation, scaleFactor);
    finalRotation = glm::normalize(finalRotation);
    return glm::toMat4(finalRotation);
}

glm::mat4 EngineBone::interpolateScale(float animationTime) {
    if (1 == scales.size()) return glm::scale(glm::mat4(1.0f), scales[0].scale);
    int p0Index = getScaleIndex(animationTime);
    int p1Index = p0Index + 1;
    float scaleFactor = getScaleFactor(scales[p0Index].timeStamp, scales[p1Index].timeStamp, animationTime);
    glm::vec3 finalScale = glm::mix(scales[p0Index].scale, scales[p1Index].scale, scaleFactor);
    return glm::scale(glm::mat4(1.0f), finalScale);
}

// 매 프레임 위치, 회전, 크기를 합쳐서 최종 변환 행렬 생성
void EngineBone::update(float animationTime) {
    glm::mat4 translation = interpolatePosition(animationTime);
    glm::mat4 rotation = interpolateRotation(animationTime);
    glm::mat4 scale = interpolateScale(animationTime);
    localTransform = translation * rotation * scale;
}


// ==============================================================================
// 2. EngineAnimation (애니메이션 전체 관리자) 구현
// ==============================================================================
EngineAnimation::EngineAnimation(const std::string& animationPath, EngineModel* model) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate);
    assert(scene && scene->mRootNode && "애니메이션 데이터를 읽을 수 없습니다!");
    
    auto animation = scene->mAnimations[0]; // 파일 내 첫 번째 애니메이션 트랙 가져오기
    duration = animation->mDuration;
    ticksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 24.0f;
    
    // 모델에서 추출해 둔 뼈대 지도(Map) 복사
    boneInfoMap = model->getBoneInfoMap(); 
    
    // 뼈대 트리 구조(부모-자식 관계) 파싱
    readHierarchyData(rootNode, scene->mRootNode);
    // 애니메이션 트랙에만 존재하는 헬퍼 뼈대들 추가 파싱
    readMissingBones(animation, *model);
}

void EngineAnimation::readMissingBones(const aiAnimation* animation, EngineModel& model) {
    int size = animation->mNumChannels;
    for (int i = 0; i < size; i++) {
        auto channel = animation->mChannels[i];
        std::string boneName = channel->mNodeName.data;

        // 모델에 없던 뼈대가 발견되면 새로 ID 부여
        if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
            boneInfoMap[boneName].id = boneInfoMap.size();
            boneInfoMap[boneName].offset = glm::mat4(1.0f);
        }
        bones.push_back(EngineBone(boneName, boneInfoMap[boneName].id, channel));
    }
}

void EngineAnimation::readHierarchyData(AssimpNodeData& dest, const aiNode* src) {
    assert(src);
    dest.name = src->mName.data;
    dest.transformation = convertMatrixToGLMFormat(src->mTransformation);
    dest.childrenCount = src->mNumChildren;

    for (int i = 0; i < src->mNumChildren; i++) {
        AssimpNodeData newData;
        readHierarchyData(newData, src->mChildren[i]);
        dest.children.push_back(newData);
    }
}

EngineBone* EngineAnimation::findBone(const std::string& name) {
    auto iter = std::find_if(bones.begin(), bones.end(),
        [&](const EngineBone& Bone) { return Bone.getBoneName() == name; }
    );
    if (iter == bones.end()) return nullptr;
    else return &(*iter);
}