#include "EngineCamera.hpp"
#include <glm/gtc/matrix_transform.hpp>

void EngineCamera::setPerspectiveProjection(float fovy, float aspect, float near, float far) {
    projectionMatrix = glm::perspective(fovy, aspect, near, far);
    // Vulkan은 OpenGL과 달리 Y축이 아래를 향하므로(Top-Down), 화면이 뒤집히지 않도록 반전시킵니다.
    projectionMatrix[1][1] *= -1.0f; 
}

void EngineCamera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
    viewMatrix = glm::lookAt(position, target, up);
}