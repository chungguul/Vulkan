#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

// ★ 추가됨: 디스크립터 셋 통신망을 통해 들어오는 전역 버퍼 (UBO)
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
} ubo;

// 개별 오브젝트의 위치/회전만 받는 푸시 상수
layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    // 카메라(전역) * 물체위치(개별) * 정점좌표
    gl_Position = ubo.projectionViewMatrix * push.modelMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
}