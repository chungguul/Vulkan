#pragma once

#include "EngineDevice.hpp"
#include "EngineTexture.hpp"
#include <vector>
#include <string>

class EngineCubemap {
public:
    EngineCubemap(EngineDevice& device, EngineTexture& hdrTexture, uint32_t resolution = 1024);
    ~EngineCubemap();

    EngineCubemap(const EngineCubemap&) = delete;
    EngineCubemap& operator=(const EngineCubemap&) = delete;

    VkImageView getImageView() const { return cubemapImageView; }
    VkSampler getSampler() const { return cubemapSampler; }

    VkImageView getIrradianceImageView() const { return irradianceImageView; }

    VkImageView getPrefilteredImageView() const { return prefilteredImageView; }

private:
    void createEmptyCubemap(uint32_t resolution, VkFormat format);
    void createCubemapImageView(VkFormat format);
    void createCubemapSampler();
    void convertFromHDR(EngineTexture& hdrTexture, uint32_t resolution, VkFormat format);
    std::vector<char> readFile(const std::string& filename);

    void createIrradianceResources(uint32_t resolution);
    void bakeIrradianceMap(uint32_t resolution);

    EngineDevice& engineDevice;
    VkImage cubemapImage = VK_NULL_HANDLE;
    VkDeviceMemory cubemapMemory = VK_NULL_HANDLE;
    VkImageView cubemapImageView = VK_NULL_HANDLE;
    VkSampler cubemapSampler = VK_NULL_HANDLE;

    VkImage irradianceImage = VK_NULL_HANDLE;
    VkDeviceMemory irradianceMemory = VK_NULL_HANDLE;
    VkImageView irradianceImageView = VK_NULL_HANDLE;

    void createPrefilteredResources(uint32_t resolution);
    void bakePrefilteredMap(uint32_t resolution);

    VkImage prefilteredImage = VK_NULL_HANDLE;
    VkDeviceMemory prefilteredMemory = VK_NULL_HANDLE;
    VkImageView prefilteredImageView = VK_NULL_HANDLE;
    
    const uint32_t maxMipLevels = 5;
};