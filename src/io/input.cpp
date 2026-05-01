#include "input.h"

#include <GLFW/glfw3.h>

namespace nenet {

Input* Input::instance_ = nullptr;

void Input::scrollCallback(GLFWwindow* , double , double yoff) {
    if (instance_) instance_->scrollAccum_ += yoff;
}

Input::Input(GLFWwindow* window) : window_(window) {
    instance_ = this;
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetCharCallback(window_, charCallback);
}

void Input::charCallback(GLFWwindow* , unsigned int codepoint) {
    if (!instance_) return;
    if (!instance_->textInputEnabled_) return;
    if (codepoint < 0x20 || codepoint > 0x7E) return;
    instance_->typedAccum_.push_back(static_cast<char>(codepoint));
}

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
    typedChars_ = std::move(typedAccum_);
    typedAccum_.clear();

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

    for (size_t i = 0; i < currMouseBtn_.size(); ++i) {
        currMouseBtn_[i] = (glfwGetMouseButton(window_, static_cast<int>(i)) == GLFW_PRESS);
    }

    scrollDelta_ = static_cast<float>(scrollAccum_);
    scrollAccum_ = 0.0;
}

void Input::endFrame() {
    prevKeys_ = currKeys_;
    prevMouseBtn_ = currMouseBtn_;
}

bool Input::isKeyDown(int key) const noexcept {
    if (key < 0 || static_cast<size_t>(key) >= currKeys_.size()) return false;
    return currKeys_[static_cast<size_t>(key)];
}

bool Input::isKeyJustPressed(int key) const noexcept {
    if (key < 0 || static_cast<size_t>(key) >= currKeys_.size()) return false;
    return currKeys_[static_cast<size_t>(key)] && !prevKeys_[static_cast<size_t>(key)];
}

bool Input::isMouseButtonDown(int button) const noexcept {
    if (button < 0 || static_cast<size_t>(button) >= currMouseBtn_.size()) return false;
    return currMouseBtn_[static_cast<size_t>(button)];
}

bool Input::isMouseButtonJustPressed(int button) const noexcept {
    if (button < 0 || static_cast<size_t>(button) >= currMouseBtn_.size()) return false;
    return currMouseBtn_[static_cast<size_t>(button)] && !prevMouseBtn_[static_cast<size_t>(button)];
}

glm::dvec2 Input::mousePosPixels() const noexcept {
    glm::dvec2 p;
    glfwGetCursorPos(window_, &p.x, &p.y);
    return p;
}

}
