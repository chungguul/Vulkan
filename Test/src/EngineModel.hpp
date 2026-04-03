#pragma once

#include "EngineDevice.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>
#include <string>

// 최대 영향을 받을 수 있는 뼈대 개수 제한
#define MAX_BONE_INFLUENCE 4

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
    //뼈대 데이터
    glm::ivec4 boneIDs{-1, -1, -1, -1}; // 영향을 주는 뼈대의 ID (기본값 -1)
    glm::vec4 boneWeights{0.0f, 0.0f, 0.0f, 0.0f}; // 각 뼈대의 영향력(가중치)

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class EngineModel {
public:
    struct Builder {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        
        void loadModel(const std::string& filepath);
    };

    EngineModel(EngineDevice& device, const EngineModel::Builder& builder);
    ~EngineModel();

    EngineModel(const EngineModel&) = delete;
    EngineModel& operator=(const EngineModel&) = delete;

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

private:
    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);

    EngineDevice& engineDevice;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t vertexCount;

    bool hasIndexBuffer = false;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;
};