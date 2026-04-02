#pragma once

#include "EngineWindow.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class EngineDevice {
public:
    EngineDevice(EngineWindow& window);
    ~EngineDevice();

    // 포인터가 꼬이는 것을 막기 위해 복사 생성자와 대입 연산자 삭제
    EngineDevice(const EngineDevice&) = delete;
    EngineDevice& operator=(const EngineDevice&) = delete;

    // 외부에서 장치 정보에 접근할 수 있도록 Getter 제공
    VkDevice getDevice() { return device; }
    VkSurfaceKHR getSurface() { return surface; }
    VkQueue getGraphicsQueue() { return graphicsQueue; }
    VkQueue getPresentQueue() { return presentQueue; }
    VkCommandPool getCommandPool() { return commandPool; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    EngineWindow& window;
    
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkCommandPool commandPool;
};