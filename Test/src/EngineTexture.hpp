#pragma once

#include "EngineDevice.hpp"
#include <string>

class EngineTexture {
public:
    EngineTexture(EngineDevice& device, const std::string& filepath);
    ~EngineTexture();

    // 복사 금지
    EngineTexture(const EngineTexture&) = delete;
    EngineTexture& operator=(const EngineTexture&) = delete;

    VkImageView getImageView() const { return textureImageView; }
    VkSampler getSampler() const { return textureSampler; }

private:
    void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);

    EngineDevice& engineDevice;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler; // 이미지를 어떻게 필터링해서 읽을지 결정하는 돋보기
};