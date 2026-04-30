#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace nenet {

class Window {
public:
    Window(uint32_t width, uint32_t height, std::string title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool shouldClose() const;
    void pollEvents() const;

    [[nodiscard]] VkSurfaceKHR createSurface(VkInstance instance) const;

    [[nodiscard]] GLFWwindow* handle() const noexcept { return window_; }
    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }

private:
    GLFWwindow* window_{nullptr};
    uint32_t width_;
    uint32_t height_;
    std::string title_;
};

}
