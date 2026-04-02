#include "KeyboardMovementController.hpp"

void KeyboardMovementController::moveInPlaneXZ(GLFWwindow* window, float dt, EngineGameObject& gameObject) {
    glm::vec3 rotate{0};
    
    // 1. 방향키 입력으로 고개 돌리기 (회전)
    if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
    if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
    if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
    if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

    // 회전값 적용 (대각선 입력 시 정규화 방지 및 속도/프레임시간 곱하기)
    if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
        gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
    }

    // 위아래로 너무 꺾이지 않도록 각도 제한 (Pitch 제한)
    gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
    // Y축 회전은 360도를 넘어가면 0으로 맞춰줌 (오버플로우 방지)
    gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

    // 2. WASD 입력으로 이동하기
    float yaw = gameObject.transform.rotation.y;
    // 현재 바라보는 방향(yaw)을 기준으로 앞으로 갈 벡터와 오른쪽으로 갈 벡터를 계산
    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
    const glm::vec3 upDir{0.f, -1.f, 0.f}; // Vulkan은 Y축이 아래를 향하므로 -1

    glm::vec3 moveDir{0.f};
    if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir -= forwardDir; // 앞으로 가려면 빼야함
    if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir += forwardDir; // 뒤로 가려면 더해야함
    if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
    if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
    if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
    if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

    // 이동 적용
    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
        gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
    }
}