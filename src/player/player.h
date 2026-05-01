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

    static constexpr int kMaxHealth = 20;
    [[nodiscard]] int health() const noexcept { return health_; }
    [[nodiscard]] int maxHealth() const noexcept { return kMaxHealth; }
    [[nodiscard]] bool isDead() const noexcept { return health_ <= 0; }
    void damage(int amount) noexcept {
        if (amount <= 0) return;
        health_ -= amount;
        if (health_ < 0) health_ = 0;
    }
    void heal(int amount) noexcept {
        if (amount <= 0) return;
        health_ += amount;
        if (health_ > kMaxHealth) health_ = kMaxHealth;
    }
    void resetHealth() noexcept { health_ = kMaxHealth; }

private:
    glm::vec3 resolveCollision(glm::vec3 motion, const BlockReader& reader) const;
    void applyCameraFromFoot();

    Camera& camera_;
    glm::vec3 footPos_{0.0f};
    glm::vec3 velocity_{0.0f};
    bool onGround_{false};
    bool flying_{true};
    double lastSpaceTime_{-10.0};
    int health_{kMaxHealth};
};

}
