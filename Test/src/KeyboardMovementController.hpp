#pragma once
#include "Components.hpp"
#include "EngineWindow.hpp"

class KeyboardMovementController {
public:
    struct KeyMappings {
        int moveLeft = GLFW_KEY_A;
        int moveRight = GLFW_KEY_D;
        int moveForward = GLFW_KEY_W;
        int moveBackward = GLFW_KEY_S;
        int moveUp = GLFW_KEY_E;
        int moveDown = GLFW_KEY_Q;
    };

    KeyMappings keys{};
    float moveSpeed{10.f};
    float lookSensitivity{0.005f};

    double lastMouseX{0.0};
    double lastMouseY{0.0};
    bool firstMouse{true};

    bool updateFreeCamera(GLFWwindow* window, float dt, TransformComponent& cameraTransform);
};