#pragma once

#include "EngineDevice.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>
#include <string> // 추가됨

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class EngineModel {
public:
    // 파일에서 데이터를 읽어올 임시 보관소 (Builder)
    struct Builder {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        
        void loadModel(const std::string& filepath);
    };

    // 기존 생성자 유지
    EngineModel(EngineDevice& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    
    // ★ 추가됨: Builder를 통해 모델을 생성하는 생성자
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