#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

// CPU에서 전달받을 푸시 상수(Push Constants) 블록
layout(push_constant) uniform Push {
    mat2 transform;
    vec2 offset;
    vec3 color;
} push;

void main() {
    // 정점 위치에 회전 행렬을 곱하고 위치를 더해줍니다.
    gl_Position = vec4(push.transform * inPosition + push.offset, 0.0, 1.0);
    fragColor = inColor;
}