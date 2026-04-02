#version 450

layout(location = 0) in vec3 inPosition; // vec2 -> vec3으로 변경
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

// 이제 4x4 행렬 하나(Projection * View * Model 결합본)만 받습니다.
layout(push_constant) uniform Push {
    mat4 transform; 
} push;

void main() {
    // 3D 위치를 행렬과 곱해 2D 화면 좌표로 변환합니다.
    gl_Position = push.transform * vec4(inPosition, 1.0);
    fragColor = inColor;
}