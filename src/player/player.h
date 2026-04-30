#pragma once

#include "../world/block_reader.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace nenet {

class Camera;

struct PlayerInput {
    glm::vec2 moveAxis{0.0f};
    glm::vec2 lookDelta{0.0f};
    bool jumpHeld{false};
    bool jumpJustPressed{false};
    bool sprint{false};
    bool flyDown{false};
};

class Player {
public:
    explicit Player(Camera& cam);

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    void update(const PlayerInput& in, const BlockReader& reader, float dt);

    void setFootPos(const glm::vec3& p);
    [[nodiscard]] glm::vec3 footPos() const noexcept { return footPos_; }
    [[nodiscard]] glm::vec3 eyePos() const noexcept;
    [[nodiscard]] bool flying() const noexcept { return flying_; }
    [[nodiscard]] bool onGround() const noexcept { return onGround_; }

    void setFlying(bool b) noexcept;

private:
    glm::vec3 resolveCollision(glm::vec3 motion, const BlockReader& reader) const;
    void applyCameraFromFoot();

    Camera& camera_;
    glm::vec3 footPos_{0.0f};
    glm::vec3 velocity_{0.0f};
    bool onGround_{false};
    bool flying_{true};
    double lastSpaceTime_{-10.0};
};

}
