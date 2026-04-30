#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <array>

struct GLFWwindow;

namespace nenet {

class Input {
public:
    explicit Input(GLFWwindow* window);

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    void beginFrame();
    void endFrame();

    [[nodiscard]] bool isKeyDown(int key) const noexcept;
    [[nodiscard]] bool isKeyJustPressed(int key) const noexcept;
    [[nodiscard]] glm::vec2 mouseDelta() const noexcept { return mouseDelta_; }

    [[nodiscard]] bool isMouseCaptured() const noexcept { return captured_; }
    void setMouseCaptured(bool captured) noexcept;

private:
    GLFWwindow* window_;
    glm::dvec2 lastMousePos_{0.0, 0.0};
    glm::vec2 mouseDelta_{0.0f, 0.0f};
    bool captured_{false};
    bool firstMouse_{true};

    std::array<bool, 512> currKeys_{};
    std::array<bool, 512> prevKeys_{};
};

}
