#include "Camera.h"
#include <iostream>

Camera::Camera()
{
    headPos = glm::vec3(0.0f, 0.0f, 0.6f);

    screenWidth = 0.312f;
    screenHeight = 0.196f;

    nearPlane = 0.05f;
    farPlane = 100.0f;
}

void Camera::setHeadPosition(const glm::vec3& pos)
{
    headPos = pos;
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::translate(
        glm::mat4(1.0f),
        glm::vec3(
            -headPos.x,
            -headPos.y,
             headPos.z
        )
    );
}

glm::mat4 Camera::getProjectionMatrix() const
{
    float near = nearPlane;
    float far = farPlane;

    const float REAL_DIST_BTWN_EYES = 0.889f; // distance between eyes in meters.

    float screenW = 0.312f;
    float screenH = 0.196f;

    float leftS   = -screenW * 0.5f;
    float rightS  =  screenW * 0.5f;
    float bottomS = -screenH * 0.5f;
    float topS    =  screenH * 0.5f;

    float l = (leftS   - headPos.x) * near / headPos.z;
    float r = (rightS  - headPos.x) * near / headPos.z;
    float b = (bottomS - headPos.y) * near / headPos.z;
    float t = (topS    - headPos.y) * near / headPos.z;

    return glm::frustum(l, r, b, t, near, far);
}