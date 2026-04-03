#include "EngineModel.hpp"
#include <cstring>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void EngineModel::Builder::loadModel(const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // .obj 파일을 읽어옵니다.
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error("실패: 모델 파일을 로드할 수 없습니다! 에러: " + warn + err);
    }

    vertices.clear();
    indices.clear();

    // 파일에 있는 모든 도형(shape)의 정점을 순회하며 우리의 Vertex 구조체로 변환합니다.
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            
            // 1. 위치(Position) 데이터
            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // 2. 색상(Color) 데이터 (파일에 색상 정보가 없으면 기본 흰색 지정)
            if (!attrib.colors.empty()) {
                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2]
                };
            } else {
                vertex.color = {1.0f, 1.0f, 1.0f}; // 흰색
            }
            // 3. 법선(Normal) 데이터
            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            } else {
                vertex.normal = {0.0f, 1.0f, 0.0f}; // 파일에 법선이 없으면 임시로 위쪽을 보게 함
            }
            // 4. UV(TexCoord) 데이터 파싱 추가
            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    // Vulkan은 이미지의 Y축이 반대이므로 1.0에서 빼주어 뒤집습니다.
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] 
                };
            } else {
                vertex.uv = {0.0f, 0.0f};
            }

            vertices.push_back(vertex);
            // 정점 중복 제거(Hash)는 나중으로 미루고, 우선은 순서대로 인덱스를 붙여줍니다.
            indices.push_back(indices.size()); 
        }
    }
}

EngineModel::EngineModel(EngineDevice& device, const EngineModel::Builder& builder) : engineDevice{device} {
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
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);
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
    
    return attributeDescriptions;
}

EngineModel::EngineModel(EngineDevice& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) : engineDevice{device} {
    createVertexBuffers(vertices);
    createIndexBuffers(indices);
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