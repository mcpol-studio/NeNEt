#include "player.h"

#include "aabb.h"
#include "../render/camera.h"
#include "../world/block.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nenet {

namespace {

constexpr float kHalfWidth = 0.30f;
constexpr float kHeight = 1.80f;
constexpr float kEyeHeight = 1.62f;
constexpr float kGravity = -28.0f;
constexpr float kMaxFallSpeed = -54.0f;
constexpr float kJumpVel = 8.4f;
constexpr float kWalkSpeed = 4.5f;
constexpr float kSprintMul = 1.6f;
constexpr float kFlySpeed = 12.0f;
constexpr float kFlySprintMul = 4.0f;
constexpr float kMouseSensitivity = 0.12f;
constexpr double kDoubleClickWindow = 0.30;

AABB makePlayerAABB(const glm::vec3& foot) {
    AABB box;
    box.min = {foot.x - kHalfWidth, foot.y, foot.z - kHalfWidth};
    box.max = {foot.x + kHalfWidth, foot.y + kHeight, foot.z + kHalfWidth};
    return box;
}

bool isCollisionBlock(Block b) {
    return b != Block::Air && b != Block::Water;
}

}

Player::Player(Camera& cam) : camera_(cam) {
    footPos_ = camera_.position() - glm::vec3(0.0f, kEyeHeight, 0.0f);
}

glm::vec3 Player::eyePos() const noexcept {
    return footPos_ + glm::vec3(0.0f, kEyeHeight, 0.0f);
}

void Player::setFootPos(const glm::vec3& p) {
    footPos_ = p;
    applyCameraFromFoot();
}

void Player::setFlying(bool b) noexcept {
    flying_ = b;
    velocity_.y = 0.0f;
}

void Player::applyCameraFromFoot() {
    camera_.setPosition(eyePos());
}

glm::vec3 Player::resolveCollision(glm::vec3 motion, const BlockReader& reader) const {
    AABB box = makePlayerAABB(footPos_);

    AABB swept;
    swept.min = glm::min(box.min, box.min + motion);
    swept.max = glm::max(box.max, box.max + motion);

    const int x0 = static_cast<int>(std::floor(swept.min.x)) - 1;
    const int x1 = static_cast<int>(std::floor(swept.max.x)) + 1;
    const int y0 = static_cast<int>(std::floor(swept.min.y)) - 1;
    const int y1 = static_cast<int>(std::floor(swept.max.y)) + 1;
    const int z0 = static_cast<int>(std::floor(swept.min.z)) - 1;
    const int z1 = static_cast<int>(std::floor(swept.max.z)) + 1;

    std::vector<AABB> blocks;
    blocks.reserve(64);
    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (!isCollisionBlock(reader(x, y, z))) continue;
                blocks.push_back({
                    glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)),
                    glm::vec3(static_cast<float>(x + 1), static_cast<float>(y + 1), static_cast<float>(z + 1))
                });
            }
        }
    }

    for (const auto& b : blocks) motion.y = clipAxisY(box, b, motion.y);
    box.min.y += motion.y;
    box.max.y += motion.y;

    for (const auto& b : blocks) motion.x = clipAxisX(box, b, motion.x);
    box.min.x += motion.x;
    box.max.x += motion.x;

    for (const auto& b : blocks) motion.z = clipAxisZ(box, b, motion.z);

    return motion;
}

void Player::update(const PlayerInput& in, const BlockReader& reader, float dt) {
    if (in.lookDelta.x != 0.0f || in.lookDelta.y != 0.0f) {
        camera_.rotate(in.lookDelta.x * kMouseSensitivity, -in.lookDelta.y * kMouseSensitivity);
    }

    if (in.jumpJustPressed) {
        const double now = glfwGetTime();
        if (now - lastSpaceTime_ < kDoubleClickWindow) {
            flying_ = !flying_;
            velocity_.y = 0.0f;
            spdlog::info("飞行模式 = {}", flying_ ? "开" : "关");
        }
        lastSpaceTime_ = now;
    }

    glm::vec2 axis = in.moveAxis;
    const float axisLen = glm::length(axis);
    if (axisLen > 1.0f) axis /= axisLen;

    const glm::vec3 fwd = camera_.forward();
    const glm::vec3 fwdFlat = glm::length(glm::vec3(fwd.x, 0.0f, fwd.z)) > 1e-4f
        ? glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z))
        : glm::vec3(0.0f);
    const glm::vec3 right = camera_.right();
    const glm::vec3 dir = fwdFlat * axis.y + right * axis.x;

    if (flying_) {
        float speed = kFlySpeed;
        if (in.sprint) speed *= kFlySprintMul;
        velocity_.x = dir.x * speed;
        velocity_.z = dir.z * speed;

        velocity_.y = 0.0f;
        if (in.jumpHeld) velocity_.y += speed;
        if (in.flyDown) velocity_.y -= speed;
    } else {
        float speed = kWalkSpeed;
        if (in.sprint) speed *= kSprintMul;
        velocity_.x = dir.x * speed;
        velocity_.z = dir.z * speed;

        velocity_.y += kGravity * dt;
        if (velocity_.y < kMaxFallSpeed) velocity_.y = kMaxFallSpeed;

        if (onGround_ && in.jumpHeld) {
            velocity_.y = kJumpVel;
            onGround_ = false;
        }
    }

    const glm::vec3 motion = velocity_ * dt;
    const glm::vec3 resolved = resolveCollision(motion, reader);

    if (std::abs(resolved.x - motion.x) > 1e-6f) velocity_.x = 0.0f;
    if (std::abs(resolved.z - motion.z) > 1e-6f) velocity_.z = 0.0f;
    if (std::abs(resolved.y - motion.y) > 1e-6f) {
        if (motion.y < 0.0f) onGround_ = true;
        velocity_.y = 0.0f;
    } else if (motion.y < 0.0f) {
        onGround_ = false;
    }

    footPos_ += resolved;
    applyCameraFromFoot();
}

}
