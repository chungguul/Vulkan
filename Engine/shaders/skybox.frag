#version 450

layout(location = 0) in vec3 inUVW;
layout(set = 0, binding = 1) uniform samplerCube skybox;

layout(location = 0) out vec4 outColor;

const float exposure = 1.0; 

void main() {
    vec3 color = texture(skybox, inUVW).rgb;
    
    vec3 mapped = vec3(1.0) - exp(-color * exposure);

    outColor = vec4(mapped, 1.0);
}