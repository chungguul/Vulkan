#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class EngineWindow {
public:
    EngineWindow(int w, int h, std::string name);
    ~EngineWindow();

    // 복사 생성자와 대입 연산자를 삭제하여 창이 실수로 복사되는 것을 방지합니다. (포인터 꼬임 방지)
    EngineWindow(const EngineWindow &) = delete;
    EngineWindow &operator=(const EngineWindow &) = delete;

    bool shouldClose() { return glfwWindowShouldClose(window); }
    void pollEvents() { glfwPollEvents(); }
    
    // 나중에 Vulkan Surface를 만들 때 필요합니다.
    GLFWwindow* getGLFWwindow() const { return window; }

    VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    bool wasWindowResized() { return framebufferResized; }
    void resetWindowResizedFlag() { framebufferResized = false; }

private:
    void initWindow();

    const int width;
    const int height;
    std::string windowName;

    bool framebufferResized = false;
    
    GLFWwindow *window;
};