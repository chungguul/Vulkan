#pragma once

#include "EngineDevice.hpp"
#include <string>

class EngineTexture {
public:
    EngineTexture(EngineDevice& device, const std::string& filepath);
    
    EngineTexture(EngineDevice& device); 
    
    ~EngineTexture();

    EngineTexture(const EngineTexture&) = delete;
    EngineTexture& operator=(const EngineTexture&) = delete;

    void loadFromFile(const std::string& filepath);
    void loadHDR(const std::string& filepath);

    VkImageView getImageView() const { return textureImageView; }
    VkSampler getSampler() const { return textureSampler; }

private:
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    void createImageView(VkFormat format);
    void createTextureSampler();

    // 포맷(Format) 인자를 추가하여 일반/HDR 모두 호환되도록 수정
    void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);

    EngineDevice& engineDevice;
    
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE; 
};