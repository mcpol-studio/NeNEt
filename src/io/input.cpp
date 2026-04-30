#include "input.h"

#include <GLFW/glfw3.h>

namespace nenet {

Input::Input(GLFWwindow* window) : window_(window) {}

void Input::setMouseCaptured(bool captured) noexcept {
    captured_ = captured;
    if (captured_) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
            glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
        firstMouse_ = true;
    } else {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Input::beginFrame() {
    if (captured_) {
        glm::dvec2 cur;
        glfwGetCursorPos(window_, &cur.x, &cur.y);
        if (firstMouse_) {
            lastMousePos_ = cur;
            firstMouse_ = false;
            mouseDelta_ = {0.0f, 0.0f};
        } else {
            mouseDelta_ = glm::vec2(cur - lastMousePos_);
            lastMousePos_ = cur;
        }
    } else {
        mouseDelta_ = {0.0f, 0.0f};
    }

    for (size_t i = 0; i < currKeys_.size(); ++i) {
        currKeys_[i] = (glfwGetKey(window_, static_cast<int>(i)) == GLFW_PRESS);
    }
}

void Input::endFrame() {
    prevKeys_ = currKeys_;
}

bool Input::isKeyDown(int key) const noexcept {
    if (key < 0 || static_cast<size_t>(key) >= currKeys_.size()) return false;
    return currKeys_[static_cast<size_t>(key)];
}

bool Input::isKeyJustPressed(int key) const noexcept {
    if (key < 0 || static_cast<size_t>(key) >= currKeys_.size()) return false;
    return currKeys_[static_cast<size_t>(key)] && !prevKeys_[static_cast<size_t>(key)];
}

}
