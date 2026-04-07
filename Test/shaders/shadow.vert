#version 450

//모델의 버텍스 입력 (위치값만 필요합니다!)
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

// 메인 렌더링과 동일한 UBO를 그대로 재활용합니다.
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionView;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 finalBonesMatrices[100];
    mat4 view;
    mat4 proj;
    // ★ 빛의 시점에서 본 투영 행렬 (C++에서 추가해 줄 예정!)
    mat4 lightSpaceMatrix; 
} ubo;

// 모델의 위치를 이동시키는 푸시 상수
layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    // 뼈대 애니메이션이 적용된 위치 계산
    mat4 boneTransform = ubo.finalBonesMatrices[0]; // (단순화를 위해 루트 뼈대만 예시로 적용)
    
    // 빛의 시점에서 본 최종 위치를 계산합니다.
    gl_Position = ubo.lightSpaceMatrix * push.modelMatrix * vec4(position, 1.0);
}