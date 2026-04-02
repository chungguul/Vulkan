#include "EngineModel.hpp"
#include <cstring>

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
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