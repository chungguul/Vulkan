#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <array>

struct FrustumPlane {
    glm::vec3 normal;
    float distance;
};

class EngineCamera {
public:
    // 원근감(Projection) 설정
    void setPerspectiveProjection(float fovy, float aspect, float near, float far);
    // 카메라 위치(View) 설정
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0.f, -1.f, 0.f));

    const glm::mat4& getProjection() const { return projectionMatrix; }
    const glm::mat4& getView() const { return viewMatrix; }
    //카메라의 위치와 회전각(Pitch, Yaw, Roll) 기반으로 뷰 행렬 계산
    void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

    std::array<FrustumPlane, 6> getFrustumPlanes() const;

private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};
};