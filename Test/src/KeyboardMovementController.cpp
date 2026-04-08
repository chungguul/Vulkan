#include "KeyboardMovementController.hpp"
#include <iostream>

bool KeyboardMovementController::updateFreeCamera(GLFWwindow* window, float dt, TransformComponent& transform) {
    bool isMoving = false;

    // ==========================================================
    // 1. 마우스 조작 (고개 돌리기)
    // ==========================================================
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (firstMouse) {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            firstMouse = false;
        }

        float xOffset = (mouseX - lastMouseX) * lookSensitivity;
        float yOffset = (lastMouseY - mouseY) * lookSensitivity; 

        lastMouseX = mouseX;
        lastMouseY = mouseY;

        transform.rotation.y += xOffset;  // 좌우 회전 (Yaw)
        transform.rotation.x += yOffset;  // 상하 회전 (Pitch)

        // 고개가 180도 뒤집히지 않도록 제한
        transform.rotation.x = glm::clamp(transform.rotation.x, -1.5f, 1.5f);
        // Y축 오버플로우 방지
        transform.rotation.y = glm::mod(transform.rotation.y, glm::two_pi<float>());
    } else {
        firstMouse = true;
    }

    // ==========================================================
    // 2. 키보드 조작 (드론 비행 이동)
    // ==========================================================
    float yaw = transform.rotation.y;
    
    // 수평(XZ) 평면 이동 벡터 계산
    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
    const glm::vec3 upDir{0.f, -1.f, 0.f}; // Vulkan은 Y축이 아래로 갈수록 커짐

    glm::vec3 moveDir{0.f};
    if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir -= forwardDir;
    if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir += forwardDir;
    if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir -= rightDir;
    if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir += rightDir;
    if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir -= upDir;   // E키: 위로
    if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir += upDir; // Q키: 아래로

    if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
        transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        isMoving = true;
    }

    return isMoving;
}