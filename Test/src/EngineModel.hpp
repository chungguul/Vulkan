#pragma once

#include "EngineDevice.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class EngineModel {
public:
    // 인덱스 배열(uint32_t)도 함께 받도록 수정
    EngineModel(EngineDevice& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~EngineModel();

    EngineModel(const EngineModel&) = delete;
    EngineModel& operator=(const EngineModel&) = delete;

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

private:
    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices); // 추가됨

    EngineDevice& engineDevice;
    
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t vertexCount;

    // 인덱스 버퍼 관련 변수 추가
    bool hasIndexBuffer = false;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;
};