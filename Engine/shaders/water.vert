#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec4 clipSpace;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragPosWorld;

layout(push_constant) uniform Push { mat4 modelMatrix; } push;

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
} ubo;

void main() {
    vec4 worldPos = push.modelMatrix * vec4(position, 1.0);
    clipSpace = ubo.projectionViewMatrix * worldPos;
    gl_Position = clipSpace;
    
    fragUV = uv * 6.0; 
    fragPosWorld = worldPos.xyz;
}