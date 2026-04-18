#version 450
struct Particle { vec3 position; vec3 velocity; vec4 color; };

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionView;
} ubo;

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