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
    void setPerspectiveProjection(float fovy, float aspect, float near, float far);
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0.f, -1.f, 0.f));

    const glm::mat4& getProjection() const { return projectionMatrix; }
    const glm::mat4& getView() const { return viewMatrix; }

    void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

    std::array<FrustumPlane, 6> getFrustumPlanes() const;

private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};
};