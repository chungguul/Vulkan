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
        int moveUp = GLFW_KEY_E;    // E키로 상승
        int moveDown = GLFW_KEY_Q;  // Q키로 하강
    };

    KeyMappings keys{};
    float moveSpeed{10.f}; // 드론 모드니까 속도를 좀 더 시원하게 올립니다!
    float lookSensitivity{0.005f};

    double lastMouseX{0.0};
    double lastMouseY{0.0};
    bool firstMouse{true};

    bool updateFreeCamera(GLFWwindow* window, float dt, TransformComponent& cameraTransform);
};