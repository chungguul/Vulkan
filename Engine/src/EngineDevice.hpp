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

    EngineDevice(const EngineDevice&) = delete;
    EngineDevice& operator=(const EngineDevice&) = delete;

    VkDevice getDevice() { return device; }
    VkSurfaceKHR getSurface() { return surface; }
    VkQueue getGraphicsQueue() { return graphicsQueue; }
    VkQueue getPresentQueue() { return presentQueue; }
    VkCommandPool getCommandPool() { return commandPool; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }

    void createBuffer(
        VkDeviceSize size, 
        VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkBuffer& buffer, 
        VkDeviceMemory& bufferMemory);

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