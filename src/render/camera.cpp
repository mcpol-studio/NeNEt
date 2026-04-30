#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace nenet {

namespace {
constexpr float kPitchLimit = 89.0f;
}

void Camera::setYawPitch(float yawDeg, float pitchDeg) noexcept {
    yawDeg_ = yawDeg;
    pitchDeg_ = std::clamp(pitchDeg, -kPitchLimit, kPitchLimit);
}

void Camera::setPerspective(float fovYRadians, float nearZ, float farZ) noexcept {
    fovY_ = fovYRadians;
    near_ = nearZ;
    far_ = farZ;
}

void Camera::lookAt(const glm::vec3& target) noexcept {
    const glm::vec3 dir = glm::normalize(target - position_);
    pitchDeg_ = glm::degrees(std::asin(dir.y));
    yawDeg_ = glm::degrees(std::atan2(dir.z, dir.x));
}

void Camera::rotate(float yawDeltaDeg, float pitchDeltaDeg) noexcept {
    yawDeg_ += yawDeltaDeg;
    pitchDeg_ = std::clamp(pitchDeg_ + pitchDeltaDeg, -kPitchLimit, kPitchLimit);
}

glm::vec3 Camera::forward() const noexcept {
    const float y = glm::radians(yawDeg_);
    const float p = glm::radians(pitchDeg_);
    return glm::normalize(glm::vec3(
        std::cos(p) * std::cos(y),
        std::sin(p),
        std::cos(p) * std::sin(y)
    ));
}

glm::vec3 Camera::right() const noexcept {
    return glm::normalize(glm::cross(forward(), worldUp()));
}

glm::mat4 Camera::view() const noexcept {
    return glm::lookAt(position_, position_ + forward(), worldUp());
}

glm::mat4 Camera::projection() const noexcept {
    glm::mat4 p = glm::perspective(fovY_, aspect_, near_, far_);
    p[1][1] *= -1.0f;
    return p;
}

}
