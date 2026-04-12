#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in ivec4 inBoneIDs;     // 뼈대 ID 4개
layout(location = 5) in vec4 inBoneWeights;  // 가중치 4개
layout(location = 6) in vec3 tangent;
layout(location = 7) in vec3 bitangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec2 fragUV;
layout(location = 4) out vec3 fragTangentWorld;
layout(location = 5) out vec3 fragBitangentWorld;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

struct PointLight {
    vec4 position; // xyz: 위치, w: 강도
    vec4 color;    // xyz: 색상
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 clipPlane;
    float time;
    PointLight pointLights[10];  // PointLight struct도 동일하게 선언 필요
    int numPointLights;
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
    // ★ [NEW] 접선 공간 변수 누적용
    vec3 totalTangent = vec3(0.0f);   
    vec3 totalBitangent = vec3(0.0f); 
    
    bool hasBones = false;
    
    for(int i = 0 ; i < 4 ; i++) {
        if(inBoneIDs[i] == -1) continue; 
        
        hasBones = true;
        if(inBoneIDs[i] >= 100) { 
            totalPosition = vec4(inPosition, 1.0f); 
            break;
        }
        
        int actualBoneIndex = (push.characterIndex * 100) + inBoneIDs[i];
        mat4 boneMatrix = boneBuffer.finalBonesMatrices[actualBoneIndex];
        mat3 boneMat3 = mat3(boneMatrix); // 회전용 3x3 행렬
        
        vec4 localPosition = boneMatrix * vec4(inPosition, 1.0f);
        totalPosition += localPosition * inBoneWeights[i];
        
        // ★ [NEW] 뼈대 애니메이션에 맞춰 T, B, N 모두 꺾어주기!
        totalNormal    += (boneMat3 * inNormal) * inBoneWeights[i];
        totalTangent   += (boneMat3 * tangent) * inBoneWeights[i];
        totalBitangent += (boneMat3 * bitangent) * inBoneWeights[i];
    }

    // 뼈대가 없는 정적 오브젝트 (Cube, Sphere 등)
    if(!hasBones) {
        totalPosition = vec4(inPosition, 1.0f);
        totalNormal = inNormal;
        // ★ [NEW] 뼈대가 없어도 원본 T, B 값을 그대로 살려줍니다!
        totalTangent = tangent;
        totalBitangent = bitangent;
    }

    vec4 positionWorld = push.modelMatrix * totalPosition;
    gl_Position = ubo.projectionViewMatrix * positionWorld;
    
    fragColor = inColor;
    fragPosWorld = positionWorld.xyz;
    fragUV = inUV;
    
    // 크기 변형(Scale) 시 노멀 왜곡을 방지하는 역전치 행렬
    mat3 normalMatrix = transpose(inverse(mat3(push.modelMatrix)));

    // ★ [NEW] 드디어 T, B, N을 완벽하게 월드 공간으로 변환하여 토스합니다!
    fragNormalWorld    = normalize(normalMatrix * totalNormal);
    fragTangentWorld   = normalize(normalMatrix * totalTangent);
    fragBitangentWorld = normalize(normalMatrix * totalBitangent);
}