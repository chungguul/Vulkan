#pragma once

#include "EngineDevice.hpp"
#include "EngineTexture.hpp"
#include <vector>
#include <string>

class EngineCubemap {
public:
    // HDR 텍스처를 받아서 지정된 해상도(기본 1024)의 큐브맵으로 변환하는 생성자
    EngineCubemap(EngineDevice& device, EngineTexture& hdrTexture, uint32_t resolution = 1024);
    ~EngineCubemap();

    EngineCubemap(const EngineCubemap&) = delete;
    EngineCubemap& operator=(const EngineCubemap&) = delete;

    VkImageView getImageView() const { return cubemapImageView; }
    VkSampler getSampler() const { return cubemapSampler; }

private:
    void createEmptyCubemap(uint32_t resolution, VkFormat format);
    void createCubemapImageView(VkFormat format);
    void createCubemapSampler();
    void convertFromHDR(EngineTexture& hdrTexture, uint32_t resolution, VkFormat format);
    std::vector<char> readFile(const std::string& filename);

    EngineDevice& engineDevice;
    VkImage cubemapImage = VK_NULL_HANDLE;
    VkDeviceMemory cubemapMemory = VK_NULL_HANDLE;
    VkImageView cubemapImageView = VK_NULL_HANDLE;
    VkSampler cubemapSampler = VK_NULL_HANDLE;
};