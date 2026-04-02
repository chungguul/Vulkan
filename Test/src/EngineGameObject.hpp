#pragma once

#include "EngineModel.hpp"
#include <memory>

// 오브젝트의 위치, 크기, 회전을 담당하는 구조체
struct Transform2dComponent {
    glm::vec2 translation{}; // 이동 (x, y)
    glm::vec2 scale{1.f, 1.f}; // 크기 (기본값 1배)
    float rotation = 0.0f;     // 회전각 (라디안)

    // 크기와 회전이 적용된 2x2 변환 행렬을 계산해서 반환합니다.
    glm::mat2 mat2() {
        float s = glm::sin(rotation);
        float c = glm::cos(rotation);
        glm::mat2 rotMatrix{c, s, -s, c};
        glm::mat2 scaleMatrix{scale.x, 0.0f, 0.0f, scale.y};
        return rotMatrix * scaleMatrix;
    }
};

class EngineGameObject {
public:
    using id_t = unsigned int;

    // 오브젝트를 생성할 때마다 겹치지 않는 고유 ID를 발급합니다.
    static EngineGameObject createGameObject() {
        static id_t currentId = 0;
        return EngineGameObject{currentId++};
    }

    // 포인터가 꼬이는 것을 막기 위해 복사는 금지하고, 소유권 이동(Move)만 허용합니다.
    EngineGameObject(const EngineGameObject&) = delete;
    EngineGameObject& operator=(const EngineGameObject&) = delete;
    EngineGameObject(EngineGameObject&&) = default;
    EngineGameObject& operator=(EngineGameObject&&) = default;

    id_t getId() const { return id; }

    // 모든 오브젝트가 똑같은 모델을 쓸 수 있으므로, 메모리 절약을 위해 shared_ptr(공유 포인터) 사용
    std::shared_ptr<EngineModel> model{};
    glm::vec3 color{};
    Transform2dComponent transform2d{};

private:
    EngineGameObject(id_t objId) : id{objId} {}
    id_t id;
};