#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class EngineCamera {
public:
    // 원근감(Projection) 설정
    void setPerspectiveProjection(float fovy, float aspect, float near, float far);
    // 카메라 위치(View) 설정
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0.f, -1.f, 0.f));

    const glm::mat4& getProjection() const { return projectionMatrix; }
    const glm::mat4& getView() const { return viewMatrix; }

private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};
};