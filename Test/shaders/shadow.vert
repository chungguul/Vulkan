#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

// ★ [NEW] 애니메이션을 위한 뼈대 ID와 가중치 입력
layout(location = 4) in ivec4 boneIds;  
layout(location = 5) in vec4 weights;   

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionView;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix; 
} ubo;

// ★ [NEW] 5번 바인딩: C++에서 넘겨준 거대한 뼈대 배열 (SSBO)
layout(std140, set = 0, binding = 5) readonly buffer BoneBuffer {
    mat4 boneMatrices[];
} boneBuffer;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    float roughness;
    float metallic;
    int characterIndex; // ★ [NEW] 현재 그리는 캐릭터의 번호
} push;

const int MAX_BONES = 100;

void main() {
    vec4 totalPosition = vec4(0.0);
    
    // 뼈대 애니메이션이 있는지 확인 (가중치의 합이 0보다 큰지)
    bool hasAnimation = (weights.x + weights.y + weights.z + weights.w) > 0.0;

    if (hasAnimation) {
        // 메인 버텍스 셰이더와 완전히 동일한 뼈대 연산!
        for(int i = 0 ; i < 4 ; i++) {
            if(boneIds[i] == -1) continue;
            if(boneIds[i] >= MAX_BONES) {
                totalPosition = vec4(position, 1.0);
                break;
            }
            
            // 내 캐릭터 번호에 맞는 뼈대 데이터 뭉치를 찾아갑니다.
            int actualBoneIndex = push.characterIndex * MAX_BONES + boneIds[i];
            vec4 localPosition = boneBuffer.boneMatrices[actualBoneIndex] * vec4(position, 1.0);
            totalPosition += localPosition * weights[i];
        }
    } else {
        // 정적 프랍(구체 등)은 원래 위치 그대로
        totalPosition = vec4(position, 1.0);
    }

    // ★ 드디어 빛의 시점에서도 코로네가 살아 숨 쉴 수 있게 되었습니다!
    gl_Position = ubo.lightSpaceMatrix * push.modelMatrix * totalPosition;
}