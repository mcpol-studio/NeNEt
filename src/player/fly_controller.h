#pragma once

namespace nenet {

class Camera;
class Input;

class FlyController {
public:
    FlyController() = default;

    void update(Camera& cam, Input& input, float dt);

    void setSpeed(float s) noexcept { speed_ = s; }
    void setBoostMultiplier(float m) noexcept { boost_ = m; }
    void setMouseSensitivity(float s) noexcept { sensitivity_ = s; }

private:
    float speed_{8.0f};
    float boost_{4.0f};
    float sensitivity_{0.12f};
};

}
