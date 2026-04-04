#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in ivec4 inBoneIDs;     // 뼈대 ID 4개
layout(location = 5) in vec4 inBoneWeights;  // 가중치 4개

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec2 fragUV;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 finalBonesMatrices[MAX_BONES]; // ★ 뼈대 행렬 100개 추가!
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    vec4 totalPosition = vec4(0.0f);
    vec3 totalNormal = vec3(0.0f);

    // ★ 스키닝(Skinning) 로직: 정점이 영향을 받는 최대 4개의 뼈대 변환을 가중치만큼 섞습니다.
    bool hasBones = false;
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        if(inBoneIDs[i] == -1) continue; // 빈칸이면 패스
        
        hasBones = true;
        if(inBoneIDs[i] >= MAX_BONES) {
            totalPosition = vec4(inPosition, 1.0f); // 안전 장치
            break;
        }
        
        // 정점 위치 꺾기
        vec4 localPosition = ubo.finalBonesMatrices[inBoneIDs[i]] * vec4(inPosition, 1.0f);
        totalPosition += localPosition * inBoneWeights[i];
        
        // 법선(빛 반사 방향)도 같이 꺾어주기 (회전만 적용되도록 mat3 사용)
        vec3 localNormal = mat3(ubo.finalBonesMatrices[inBoneIDs[i]]) * inNormal;
        totalNormal += localNormal * inBoneWeights[i];
    }

    // 뼈대 데이터가 아예 없는 정점이라면 원래 위치 그대로 사용
    if(!hasBones) {
        totalPosition = vec4(inPosition, 1.0f);
        totalNormal = inNormal;
    }

    vec4 positionWorld = push.modelMatrix * totalPosition;
    gl_Position = ubo.projectionViewMatrix * positionWorld;

    fragColor = inColor;
    fragPosWorld = positionWorld.xyz;
    fragNormalWorld = normalize(mat3(push.modelMatrix) * totalNormal);
    fragUV = inUV;
}