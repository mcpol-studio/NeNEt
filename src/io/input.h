#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <array>
#include <string>

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

    [[nodiscard]] bool isMouseButtonDown(int button) const noexcept;
    [[nodiscard]] bool isMouseButtonJustPressed(int button) const noexcept;

    [[nodiscard]] glm::vec2 mouseDelta() const noexcept { return mouseDelta_; }
    [[nodiscard]] glm::dvec2 mousePosPixels() const noexcept;
    [[nodiscard]] float scrollDelta() const noexcept { return scrollDelta_; }

    [[nodiscard]] bool isMouseCaptured() const noexcept { return captured_; }
    void setMouseCaptured(bool captured) noexcept;

    [[nodiscard]] const std::string& typedChars() const noexcept { return typedChars_; }
    void setTextInputEnabled(bool b) noexcept { textInputEnabled_ = b; }

private:
    GLFWwindow* window_;
    glm::dvec2 lastMousePos_{0.0, 0.0};
    glm::vec2 mouseDelta_{0.0f, 0.0f};
    bool captured_{false};
    bool firstMouse_{true};

    static constexpr size_t kKeyCount = 512;
    static constexpr size_t kMouseButtonCount = 8;

    std::array<bool, kKeyCount> currKeys_{};
    std::array<bool, kKeyCount> prevKeys_{};
    std::array<bool, kMouseButtonCount> currMouseBtn_{};
    std::array<bool, kMouseButtonCount> prevMouseBtn_{};

    double scrollAccum_{0.0};
    float scrollDelta_{0.0f};

    bool textInputEnabled_{false};
    std::string typedChars_;
    std::string typedAccum_;

    static Input* instance_;
    static void scrollCallback(GLFWwindow* w, double xoff, double yoff);
    static void charCallback(GLFWwindow* w, unsigned int codepoint);
};

}
