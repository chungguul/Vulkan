#include "EngineModel.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


static glm::mat4 convertMatrixToGLMFormat(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

void EngineModel::Builder::loadModel(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath, 
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error("실패: Assimp 모델 로드 오류 - " + std::string(importer.GetErrorString()));
    }

    vertices.clear();
    indices.clear();

    //파일 안의 모든 부품(메쉬)을 순회
    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        aiMesh* mesh = scene->mMeshes[m];
        
        // 현재 메쉬의 정점들이 시작될 위치 (오프셋)
        uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());

        // 정점 파싱 
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex{};
            vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            if (mesh->HasNormals()) vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            if (mesh->mTextureCoords[0]) vertex.uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            else vertex.uv = { 0.0f, 0.0f };
            vertex.color = { 1.0f, 1.0f, 1.0f };
        
            //Assimp가 계산해둔 Tangent와 Bitangent 데이터
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
                vertex.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
            } else {
                // 노말이나 UV 기본값
                vertex.tangent = { 1.0f, 0.0f, 0.0f };
                vertex.bitangent = { 0.0f, 1.0f, 0.0f };
            }

            vertices.push_back(vertex);
        }
        extractBoneWeightForVertices(mesh, scene, vertexOffset);

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j] + vertexOffset);
            }
        }
    }
}

EngineModel::EngineModel(EngineDevice& device, const EngineModel::Builder& builder) 
    : engineDevice{device}, boneInfoMap{builder.boneInfoMap}, boneCounter{builder.boneCounter} {
    
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);

    boundingCenter = builder.boundingCenter;
    boundingRadius = builder.boundingRadius;
}

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(8);
    // 위치
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);
    // 색상
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    // 법선
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, normal);
    // 텍스쳐
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, uv);
    // Bone IDs
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SINT; 
    attributeDescriptions[4].offset = offsetof(Vertex, boneIDs);
    // Bone Weights
    attributeDescriptions[5].binding = 0;
    attributeDescriptions[5].location = 5;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = offsetof(Vertex, boneWeights);
    //Tangent
    attributeDescriptions[6].binding = 0;
    attributeDescriptions[6].location = 6;
    attributeDescriptions[6].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[6].offset = offsetof(Vertex, tangent);
    //Bitangent
    attributeDescriptions[7].binding = 0;
    attributeDescriptions[7].location = 7;
    attributeDescriptions[7].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[7].offset = offsetof(Vertex, bitangent);


    return attributeDescriptions;
}


EngineModel::~EngineModel() {
    vkDestroyBuffer(engineDevice.getDevice(), vertexBuffer, nullptr);
    vkFreeMemory(engineDevice.getDevice(), vertexBufferMemory, nullptr);

    if (hasIndexBuffer) {
        vkDestroyBuffer(engineDevice.getDevice(), indexBuffer, nullptr);
        vkFreeMemory(engineDevice.getDevice(), indexBufferMemory, nullptr);
    }
}

void EngineModel::createVertexBuffers(const std::vector<Vertex>& vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    //버퍼 객체 생성
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(engineDevice.getDevice(), &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("실패: 버텍스 버퍼 생성 오류!");
    }

    //버퍼에 필요한 메모리 요구사항 확인 후 실제 메모리 할당
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engineDevice.getDevice(), vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = engineDevice.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("실패: 버텍스 버퍼 메모리 할당 오류!");
    }
    vkBindBufferMemory(engineDevice.getDevice(), vertexBuffer, vertexBufferMemory, 0);

    // CPU 데이터를 GPU 메모리로 복사
    void* data;
    vkMapMemory(engineDevice.getDevice(), vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(engineDevice.getDevice(), vertexBufferMemory);
}

void EngineModel::createIndexBuffers(const std::vector<uint32_t>& indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;

    if (!hasIndexBuffer) return;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(engineDevice.getDevice(), &bufferInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("실패: 인덱스 버퍼 생성 오류!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engineDevice.getDevice(), indexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = engineDevice.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("실패: 인덱스 버퍼 메모리 할당 오류!");
    }
    vkBindBufferMemory(engineDevice.getDevice(), indexBuffer, indexBufferMemory, 0);

    void* data;
    vkMapMemory(engineDevice.getDevice(), indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(engineDevice.getDevice(), indexBufferMemory);
}

void EngineModel::bind(VkCommandBuffer commandBuffer) {
    VkBuffer buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

void EngineModel::draw(VkCommandBuffer commandBuffer) {
    if (hasIndexBuffer) {
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
}

void EngineModel::Builder::setVertexBoneData(Vertex& vertex, int boneID, float weight) {
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        if (vertex.boneIDs[i] < 0) {
            vertex.boneWeights[i] = weight;
            vertex.boneIDs[i] = boneID;
            break;
        }
    }
}

void EngineModel::Builder::extractBoneWeightForVertices(aiMesh* mesh, const aiScene* scene, uint32_t vertexOffset) {
    for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        // 처음 보는 뼈라면 Map에 새로 등록
        if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
            //std::cout << boneName << std::endl;
            BoneInfo newBoneInfo;
            newBoneInfo.id = boneCounter;
            newBoneInfo.offset = convertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
            
            boneInfoMap[boneName] = newBoneInfo;
            boneID = boneCounter;
            boneCounter++;
        } else { 
            boneID = boneInfoMap[boneName].id;
        }

        assert(boneID != -1);

        aiVector3D* aiWeights = mesh->mVertices;
        aiBone* bone = mesh->mBones[boneIndex];
        
        for (int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
            int vertexId = bone->mWeights[weightIndex].mVertexId + vertexOffset;
            float weight = bone->mWeights[weightIndex].mWeight;
            
            assert(vertexId <= vertices.size());
            
            if (weight == 0.0f) continue;

            setVertexBoneData(vertices[vertexId], boneID, weight);
        }
    }
}

void EngineModel::Builder::calculateBoundingSphere() {
    if (vertices.empty()) return;

    glm::vec3 minAABB = vertices[0].position;
    glm::vec3 maxAABB = vertices[0].position;

    for (const auto& v : vertices) {
        minAABB = glm::min(minAABB, v.position);
        maxAABB = glm::max(maxAABB, v.position);
    }
    boundingCenter = (minAABB + maxAABB) / 2.0f;

    float maxDistSq = 0.0f;
    for (const auto& v : vertices) {
        glm::vec3 diff = v.position - boundingCenter;
        float distSq = glm::dot(diff, diff);
        maxDistSq = std::max(maxDistSq, distSq);
    }
    boundingRadius = std::sqrt(maxDistSq);
}