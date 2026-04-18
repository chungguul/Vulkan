#pragma once

#include "EngineDevice.hpp"
#include "EngineModel.hpp"
#include "EngineTexture.hpp"

#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>

class AssetManager {
private:
    EngineDevice& engineDevice;
    std::unordered_map<std::string, std::shared_ptr<EngineModel>> models;
    std::unordered_map<std::string, std::shared_ptr<EngineTexture>> textures;

public:
    AssetManager(EngineDevice& device) : engineDevice{device} {}

    // 복사 방지
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void loadModel(const std::string& name, const std::string& path);
    std::shared_ptr<EngineModel> getModel(const std::string& name);

    void loadTexture(const std::string& name, const std::string& filepath);
    std::shared_ptr<EngineTexture> getTexture(const std::string& name);
};