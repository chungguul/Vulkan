#pragma once

#include "EngineDevice.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>

#define MAX_BONE_INFLUENCE 4

struct aiMesh;
struct aiScene;

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
    //뼈대 데이터
    glm::ivec4 boneIDs{-1, -1, -1, -1};
    glm::vec4 boneWeights{0.0f, 0.0f, 0.0f, 0.0f};

    glm::vec3 tangent{0.0f};
    glm::vec3 bitangent{0.0f};

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct BoneInfo {
    int id;
    glm::mat4 offset;
};

class EngineModel {
public:
    struct Builder {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        
        std::map<std::string, BoneInfo> boneInfoMap{};
        int boneCounter = 0;

        glm::vec3 boundingCenter{0.0f};
        float boundingRadius{0.0f};

        void loadModel(const std::string& filepath);

        void calculateBoundingSphere();

    private:
        void extractBoneWeightForVertices(aiMesh* mesh, const aiScene* scene, uint32_t vertexOffset);
        void setVertexBoneData(Vertex& vertex, int boneID, float weight);

    };

    EngineModel(EngineDevice& device, const EngineModel::Builder& builder);
    ~EngineModel();

    EngineModel(const EngineModel&) = delete;
    EngineModel& operator=(const EngineModel&) = delete;

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

    const std::map<std::string, BoneInfo>& getBoneInfoMap() const { return boneInfoMap; }
    int getBoneCount() const { return boneCounter; }

    glm::vec3 getBoundingCenter() const { return boundingCenter; }
    float getBoundingRadius() const { return boundingRadius; }

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

    std::map<std::string, BoneInfo> boneInfoMap{};
    int boneCounter = 0;

    glm::vec3 boundingCenter{0.0f};
    float boundingRadius{0.0f};
};