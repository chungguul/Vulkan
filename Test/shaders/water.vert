#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec4 clipSpace; // ★ 프래그먼트 셰이더로 보낼 화면 좌표
layout(location = 1) out vec2 fragUV;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 finalBonesMatrices[100];
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 clipPlane;
    float time; 
} ubo;

void main() {
    vec4 worldPos = push.modelMatrix * vec4(position, 1.0);
    
    clipSpace = ubo.projectionViewMatrix * worldPos;
    gl_Position = clipSpace;
    
    fragUV = uv;
}