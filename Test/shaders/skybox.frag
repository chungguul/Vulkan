#version 450

layout(location = 0) in vec3 inUVW;
layout(set = 0, binding = 1) uniform samplerCube skybox;

layout(location = 0) out vec4 outColor;

// 노출(Exposure) 값입니다. 
// 원본 이미지가 너무 밝으면 0.8, 너무 어두우면 1.2 등 입맛에 맞게 조절할 수 있습니다.
const float exposure = 1.0; 

void main() {
    // 1. 큐브맵에서 원본 HDR 색상 가져오기
    vec3 color = texture(skybox, inUVW).rgb;
    
    // 2. Exposure 톤매핑 (Reinhard보다 원본의 쨍한 색감을 훨씬 잘 살려줍니다)
    vec3 mapped = vec3(1.0) - exp(-color * exposure);
    
    // 3. 감마 보정 삭제! (스왑체인이 SRGB이므로 하드웨어에 맡깁니다)
    // mapped = pow(mapped, vec3(1.0/2.2)); <-- 이중 감마의 원흉이므로 사용하지 않습니다.

    outColor = vec4(mapped, 1.0);
}