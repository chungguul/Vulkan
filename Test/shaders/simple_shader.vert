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
} ubo;

layout(std140, set = 0, binding = 5) readonly buffer BoneBuffer {
    mat4 finalBonesMatrices[]; // 배열 크기 제한 없음!
} boneBuffer;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    float roughness;
    float metallic;
    int characterIndex;
} push;

void main() {
    vec4 totalPosition = vec4(0.0f);
    vec3 totalNormal = vec3(0.0f);

    bool hasBones = false;
    
    // MAX_BONE_INFLUENCE는 보통 4입니다.
    for(int i = 0 ; i < 4 ; i++) {
        if(inBoneIDs[i] == -1) continue; // 빈칸이면 패스
        
        hasBones = true;
        if(inBoneIDs[i] >= 100) { // MAX_BONES 안전 장치
            totalPosition = vec4(inPosition, 1.0f); 
            break;
        }
        
        // ==========================================================
        // ★ 핵심 변경: SSBO에서 내 캐릭터의 뼈대 위치를 찾아옵니다!
        // 내 번호표(push.characterIndex) * 100개 + 현재 뼈대 번호
        int actualBoneIndex = (push.characterIndex * 100) + inBoneIDs[i];
        
        // ubo 대신 boneBuffer(SSBO)에서 행렬을 꺼냅니다.
        mat4 boneMatrix = boneBuffer.finalBonesMatrices[actualBoneIndex];
        // ==========================================================
        
        // 정점 위치 꺾기
        vec4 localPosition = boneMatrix * vec4(inPosition, 1.0f);
        totalPosition += localPosition * inBoneWeights[i];
        
        // 법선(빛 반사 방향) 꺾기 (회전만 적용되도록 mat3 사용)
        vec3 localNormal = mat3(boneMatrix) * inNormal;
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