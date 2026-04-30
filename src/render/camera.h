#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace nenet {

class Camera {
public:
    Camera() = default;

    void setAspect(float a) noexcept { aspect_ = a; }
    void setPosition(const glm::vec3& p) noexcept { position_ = p; }
    void setYawPitch(float yawDeg, float pitchDeg) noexcept;
    void setPerspective(float fovYRadians, float nearZ, float farZ) noexcept;
    void lookAt(const glm::vec3& target) noexcept;

    void rotate(float yawDeltaDeg, float pitchDeltaDeg) noexcept;
    void translate(const glm::vec3& worldOffset) noexcept { position_ += worldOffset; }

    [[nodiscard]] glm::vec3 position() const noexcept { return position_; }
    [[nodiscard]] float yaw() const noexcept { return yawDeg_; }
    [[nodiscard]] float pitch() const noexcept { return pitchDeg_; }

    [[nodiscard]] glm::vec3 forward() const noexcept;
    [[nodiscard]] glm::vec3 right() const noexcept;
    [[nodiscard]] static glm::vec3 worldUp() noexcept { return {0.0f, 1.0f, 0.0f}; }

    [[nodiscard]] glm::mat4 view() const noexcept;
    [[nodiscard]] glm::mat4 projection() const noexcept;
    [[nodiscard]] glm::mat4 viewProj() const noexcept { return projection() * view(); }

private:
    glm::vec3 position_{8.0f, 8.0f, 28.0f};
    float yawDeg_{-90.0f};
    float pitchDeg_{-15.0f};
    float fovY_{glm::radians(70.0f)};
    float aspect_{16.0f / 9.0f};
    float near_{0.1f};
    float far_{1000.0f};
};

}
