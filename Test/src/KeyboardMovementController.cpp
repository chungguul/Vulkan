#include "KeyboardMovementController.hpp"
#include <iostream>

bool KeyboardMovementController::updateFreeCamera(GLFWwindow* window, float dt, TransformComponent& transform) {
    bool isMoving = false;

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

        transform.rotation.y += xOffset;
        transform.rotation.x += yOffset;


        transform.rotation.x = glm::clamp(transform.rotation.x, -1.5f, 1.5f);
  
        transform.rotation.y = glm::mod(transform.rotation.y, glm::two_pi<float>());
    } else {
        firstMouse = true;
    }


    float yaw = transform.rotation.y;
    

    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
    const glm::vec3 upDir{0.f, -1.f, 0.f};

    glm::vec3 moveDir{0.f};
    if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir -= forwardDir;
    if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir += forwardDir;
    if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir -= rightDir;
    if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir += rightDir;
    if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir -= upDir;
    if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir += upDir; 

    if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
        transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        isMoving = true;
    }

    return isMoving;
}