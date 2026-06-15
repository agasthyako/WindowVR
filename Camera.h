#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:

    Camera();

    void setHeadPosition(const glm::vec3& pos);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

private:

    glm::vec3 headPos;

    float screenWidth;
    float screenHeight;

    float nearPlane;
    float farPlane;
};