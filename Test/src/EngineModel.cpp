#include "EngineModel.hpp"
#include <cstring>

#define TINYOBJLOADER_IMPLEMENTATION
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

static glm::mat4 convertMatrixToGLMFormat(const aiMatrix4x4& from) {
    glm::mat4 to;
    // Assimp는 Row-major, GLM은 Column-major이므로 행과 열을 뒤집어서 복사합니다.
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

void EngineModel::Builder::loadModel(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath, 
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error("실패: Assimp 모델 로드 오류 - " + std::string(importer.GetErrorString()));
    }

    vertices.clear();
    indices.clear();

    //파일 안의 모든 부품(메쉬)을 순회합니다.
    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        aiMesh* mesh = scene->mMeshes[m];
        
        // 현재 메쉬의 정점들이 시작될 위치 (오프셋)
        uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());

        // --- 정점 파싱 ---
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex{};
            vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            if (mesh->HasNormals()) vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            if (mesh->mTextureCoords[0]) vertex.uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            else vertex.uv = { 0.0f, 0.0f };
            vertex.color = { 1.0f, 1.0f, 1.0f };
            
            vertices.push_back(vertex);
        }

        // --- 뼈대 가중치 추출 (오프셋 같이 넘겨주기) ---
        extractBoneWeightForVertices(mesh, scene, vertexOffset);

        // --- 인덱스 파싱 ---
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                // ★ 수정됨: 인덱스에도 정점 오프셋을 더해주어야 올바른 정점을 가리킵니다.
                indices.push_back(face.mIndices[j] + vertexOffset);
            }
        }
    }
}

EngineModel::EngineModel(EngineDevice& device, const EngineModel::Builder& builder) 
    : engineDevice{device}, boneInfoMap{builder.boneInfoMap}, boneCounter{builder.boneCounter} {
    
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
}

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(6);
    // 위치(Position) 데이터 설명
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);
    // 색상(Color) 데이터 설명
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    // 법선(Noraml) 데이터 설명
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2; // 셰이더의 location = 2 에 매핑
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, normal);
    // 텍스쳐(UV) 데이터 설명
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3; // 셰이더의 location = 3
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT; // vec2이므로 R32G32
    attributeDescriptions[3].offset = offsetof(Vertex, uv);
    // Bone IDs (정수형 데이터이므로 SINT 사용)
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SINT; 
    attributeDescriptions[4].offset = offsetof(Vertex, boneIDs);
    // Bone Weights (실수형)
    attributeDescriptions[5].binding = 0;
    attributeDescriptions[5].location = 5;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = offsetof(Vertex, boneWeights);


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

    // 1. 버퍼 객체 생성
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(engineDevice.getDevice(), &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("실패: 버텍스 버퍼 생성 오류!");
    }

    // 2. 버퍼에 필요한 메모리 요구사항 확인 후 실제 메모리 할당
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engineDevice.getDevice(), vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // CPU에서 쓸 수 있는(HOST_VISIBLE) 메모리 영역을 찾습니다.
    allocInfo.memoryTypeIndex = engineDevice.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(engineDevice.getDevice(), &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("실패: 버텍스 버퍼 메모리 할당 오류!");
    }
    vkBindBufferMemory(engineDevice.getDevice(), vertexBuffer, vertexBufferMemory, 0);

    // 3. CPU 데이터를 GPU 메모리로 복사 (Mapping)
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
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // 인덱스 버퍼 명시
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

    // 인덱스 버퍼가 존재하면 함께 바인딩합니다. (32비트 uint 자료형 사용)
    if (hasIndexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

void EngineModel::draw(VkCommandBuffer commandBuffer) {
    if (hasIndexBuffer) {
        // 인덱스 버퍼를 사용한 그리기 명령!
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
}

void EngineModel::Builder::setVertexBoneData(Vertex& vertex, int boneID, float weight) {
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        if (vertex.boneIDs[i] < 0) { // 빈 칸을 찾으면
            vertex.boneWeights[i] = weight;
            vertex.boneIDs[i] = boneID;
            break; // 하나 넣었으면 다음 뼈대를 위해 탈출!
        }
    }
}

void EngineModel::Builder::extractBoneWeightForVertices(aiMesh* mesh, const aiScene* scene, uint32_t vertexOffset) {
    for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        // 처음 보는 뼈라면 Map에 새로 등록
        if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
            BoneInfo newBoneInfo;
            newBoneInfo.id = boneCounter;
            newBoneInfo.offset = convertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
            
            boneInfoMap[boneName] = newBoneInfo;
            boneID = boneCounter;
            boneCounter++;
        } else { // 이미 등록된 뼈라면 ID만 가져옴
            boneID = boneInfoMap[boneName].id;
        }

        assert(boneID != -1);

        // 이 뼈가 영향을 주는 모든 정점(Vertex)을 찾아가서 가중치를 주사합니다!
        aiVector3D* aiWeights = mesh->mVertices;
        aiBone* bone = mesh->mBones[boneIndex];
        
        for (int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
            int vertexId = bone->mWeights[weightIndex].mVertexId + vertexOffset;
            float weight = bone->mWeights[weightIndex].mWeight;
            
            assert(vertexId <= vertices.size());
            
            // 가중치가 0인 쓰레기 데이터는 무시
            if (weight == 0.0f) continue;

            setVertexBoneData(vertices[vertexId], boneID, weight);
        }
    }
}