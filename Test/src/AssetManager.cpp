#include "AssetManager.hpp"

void AssetManager::loadTexture(const std::string& name, const std::string& filepath) {
    textures[name] = std::make_shared<EngineTexture>(engineDevice, filepath);
}

std::shared_ptr<EngineTexture> AssetManager::getTexture(const std::string& name) {
    auto it = textures.find(name);
    if (it != textures.end()) return it->second;
    throw std::runtime_error("텍스처를 찾을 수 없습니다: " + name);
}

void AssetManager::loadModel(const std::string& name, const std::string& filepath) {
    EngineModel::Builder builder{};

    if (filepath == "Primitive:Plane") {
        builder.vertices = {
            {{-20.0f, 0.0f, -20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-20.0f, 0.0f,  20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 10.0f}},
            {{ 20.0f, 0.0f,  20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {10.0f, 10.0f}},
            {{ 20.0f, 0.0f, -20.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {10.0f, 0.0f}}
        };
        builder.indices = {0, 1, 2, 2, 3, 0};
    } 
    else if (filepath == "Primitive:Cube") {
        builder.vertices = {
            {{-0.5f,-0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f, 0.f, 1.f},{0.f,1.f}}, {{ 0.5f,-0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f, 0.f, 1.f},{1.f,1.f}}, {{ 0.5f, 0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f, 0.f, 1.f},{1.f,0.f}}, {{-0.5f, 0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f, 0.f, 1.f},{0.f,0.f}}, // Front
            {{-0.5f,-0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f, 0.f,-1.f},{1.f,1.f}}, {{ 0.5f,-0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f, 0.f,-1.f},{0.f,1.f}}, {{ 0.5f, 0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f, 0.f,-1.f},{0.f,0.f}}, {{-0.5f, 0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f, 0.f,-1.f},{1.f,0.f}}, // Back
            {{-0.5f, 0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f, 1.f, 0.f},{0.f,1.f}}, {{ 0.5f, 0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f, 1.f, 0.f},{1.f,1.f}}, {{ 0.5f, 0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f, 1.f, 0.f},{1.f,0.f}}, {{-0.5f, 0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f, 1.f, 0.f},{0.f,0.f}}, // Top
            {{-0.5f,-0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f,-1.f, 0.f},{0.f,0.f}}, {{ 0.5f,-0.5f, 0.5f},{1.f,1.f,1.f},{ 0.f,-1.f, 0.f},{1.f,0.f}}, {{ 0.5f,-0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f,-1.f, 0.f},{1.f,1.f}}, {{-0.5f,-0.5f,-0.5f},{1.f,1.f,1.f},{ 0.f,-1.f, 0.f},{0.f,1.f}}, // Bottom
            {{ 0.5f,-0.5f, 0.5f},{1.f,1.f,1.f},{ 1.f, 0.f, 0.f},{0.f,1.f}}, {{ 0.5f,-0.5f,-0.5f},{1.f,1.f,1.f},{ 1.f, 0.f, 0.f},{1.f,1.f}}, {{ 0.5f, 0.5f,-0.5f},{1.f,1.f,1.f},{ 1.f, 0.f, 0.f},{1.f,0.f}}, {{ 0.5f, 0.5f, 0.5f},{1.f,1.f,1.f},{ 1.f, 0.f, 0.f},{0.f,0.f}}, // Right
            {{-0.5f,-0.5f, 0.5f},{1.f,1.f,1.f},{-1.f, 0.f, 0.f},{1.f,1.f}}, {{-0.5f,-0.5f,-0.5f},{1.f,1.f,1.f},{-1.f, 0.f, 0.f},{0.f,1.f}}, {{-0.5f, 0.5f,-0.5f},{1.f,1.f,1.f},{-1.f, 0.f, 0.f},{0.f,0.f}}, {{-0.5f, 0.5f, 0.5f},{1.f,1.f,1.f},{-1.f, 0.f, 0.f},{1.f,0.f}}  // Left
        };
        builder.indices = { 0,1,2,2,3,0, 5,4,7,7,6,5, 8,9,10,10,11,8, 15,14,13,13,12,15, 16,17,18,18,19,16, 21,20,23,23,22,21 };
        // ★ 조기 퇴근(return) 삭제됨!
    }
    else if (filepath == "Primitive:Sphere") {
        const int sectors = 36;
        const int stacks = 18;
        const float PI = 3.14159265359f;
        for (int i = 0; i <= stacks; ++i) {
            float V = (float)i / stacks;
            float phi = V * PI;
            for (int j = 0; j <= sectors; ++j) {
                float U = (float)j / sectors;
                float theta = U * 2.0f * PI;
                float x = std::cos(theta) * std::sin(phi);
                float y = std::cos(phi);
                float z = std::sin(theta) * std::sin(phi);
                builder.vertices.push_back({{x, y, z}, {1.0f, 1.0f, 1.0f}, {x, y, z}, {U, V}});
            }
        }
        // ★ 날아갔던 구(Sphere) 인덱스 생성 코드 복구!
        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < sectors; ++j) {
                int first = (i * (sectors + 1)) + j;
                int second = first + sectors + 1;
                builder.indices.insert(builder.indices.end(), { 
                    (uint32_t)first, (uint32_t)(first + 1), (uint32_t)second, 
                    (uint32_t)(first + 1), (uint32_t)(second + 1), (uint32_t)second 
                });
            }
        }
    }
    else {
        // 일반 외부 모델 로딩
        builder.loadModel(filepath);
    }

    for (auto& v : builder.vertices) {
        if (glm::length(v.tangent) < 0.01f) {
            // 법선(Normal)과 겹치지 않는 임의의 Up 벡터 선정
            glm::vec3 up = std::abs(v.normal.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
            
            // 외적(Cross)을 통해 완벽한 수직 벡터들을 창조!
            v.tangent = glm::normalize(glm::cross(up, v.normal));
            v.bitangent = glm::normalize(glm::cross(v.normal, v.tangent));
        }
    }

    // ★ Plane, Cube, Sphere, 외부 모델 모두 사이좋게 여기서 한 번에 Bounding Sphere를 계산합니다.
    builder.calculateBoundingSphere();
    models[name] = std::make_shared<EngineModel>(engineDevice, builder); // device가 아니라 engineDevice!
}

std::shared_ptr<EngineModel> AssetManager::getModel(const std::string& name) {
    auto it = models.find(name);
    if (it != models.end()) return it->second;
    throw std::runtime_error("모델을 찾을 수 없습니다: " + name);
}