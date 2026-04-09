#version 450
struct Particle { vec3 position; vec3 velocity; vec4 color; };

// 바인딩 0번: 카메라 행렬을 읽습니다.
layout(binding = 0) uniform GlobalUbo {
    mat4 projectionView;
} ubo;

// 바인딩 1번: 컴퓨트가 갱신한 파티클 위치를 읽습니다.
layout(std430, binding = 1) readonly buffer ParticleSSBO {
    Particle particles[];
};

layout(location = 0) out vec4 fragColor;

void main() {
    Particle p = particles[gl_VertexIndex];
    gl_Position = ubo.projectionView * vec4(p.position, 1.0);
    gl_PointSize = 4.0; 
    fragColor = p.color;
}