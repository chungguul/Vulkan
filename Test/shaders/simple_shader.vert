#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal; // 추가된 속성

// 프래그먼트 셰이더로 넘겨줄 데이터들
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.projectionViewMatrix * positionWorld;

    fragColor = inColor;
    fragPosWorld = positionWorld.xyz;

    // 표면의 방향(법선)도 물체와 함께 회전시킵니다.
    fragNormalWorld = normalize(mat3(push.modelMatrix) * inNormal);
}