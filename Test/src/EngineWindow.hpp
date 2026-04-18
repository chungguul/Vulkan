#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class EngineWindow {
public:
    EngineWindow(int w, int h, std::string name);
    ~EngineWindow();

    EngineWindow(const EngineWindow &) = delete;
    EngineWindow &operator=(const EngineWindow &) = delete;

    bool shouldClose() { return glfwWindowShouldClose(window); }
    void pollEvents() { glfwPollEvents(); }
    
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