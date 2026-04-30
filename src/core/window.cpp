#include "window.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

namespace nenet {

Window::Window(uint32_t width, uint32_t height, std::string title)
    : width_(width), height_(height), title_(std::move(title)) {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("glfwInit 失败");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ = glfwCreateWindow(static_cast<int>(width_),
                                static_cast<int>(height_),
                                title_.c_str(),
                                nullptr,
                                nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow 失败");
    }

    spdlog::info("窗口已创建 {}x{} 标题=\"{}\"", width_, height_, title_);
}

Window::~Window() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    spdlog::info("窗口已销毁");
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void Window::pollEvents() const {
    glfwPollEvents();
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    const VkResult res = glfwCreateWindowSurface(instance, window_, nullptr, &surface);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("glfwCreateWindowSurface 失败");
    }
    return surface;
}

}
