#include "app.h"

#include "logger.h"
#include "window.h"
#include "../io/input.h"
#include "../player/fly_controller.h"
#include "../render/camera.h"
#include "../render/chunk_mesh.h"
#include "../render/renderer.h"
#include "../render/swapchain.h"
#include "../render/vulkan_context.h"
#include "../world/chunk.h"
#include "../world/chunk_mesher.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <chrono>

namespace nenet {

App::App() {
    log::init();
    spdlog::info("NeNEt 启动");

    window_ = std::make_unique<Window>(1280, 720, "NeNEt");
    vk_ = std::make_unique<VulkanContext>(*window_);
    swapchain_ = std::make_unique<Swapchain>(*vk_, window_->width(), window_->height());

    camera_ = std::make_unique<Camera>();
    camera_->setAspect(static_cast<float>(window_->width()) / static_cast<float>(window_->height()));
    camera_->setPosition({Chunk::kSizeX * 0.5f, 10.0f, Chunk::kSizeZ + 12.0f});
    camera_->lookAt({Chunk::kSizeX * 0.5f, 4.0f, Chunk::kSizeZ * 0.5f});

    chunk_ = std::make_unique<Chunk>();
    chunk_->fillTestTerrain();
    auto meshData = NaiveMesher::mesh(*chunk_);
    chunkMesh_ = std::make_unique<ChunkMesh>(vk_->allocator(), meshData);

    renderer_ = std::make_unique<Renderer>(*vk_, *swapchain_, *camera_, *chunkMesh_);

    input_ = std::make_unique<Input>(window_->handle());
    controller_ = std::make_unique<FlyController>();
    input_->setMouseCaptured(true);

    spdlog::info("控制：WASD 移动 / Space 上 / Ctrl 下 / Shift 加速 / 鼠标视角 / ESC 退出");
}

App::~App() {
    if (vk_) vk_->waitIdle();
    renderer_.reset();
    chunkMesh_.reset();
    chunk_.reset();
    controller_.reset();
    input_.reset();
    camera_.reset();
    swapchain_.reset();
    vk_.reset();
    window_.reset();
    spdlog::info("NeNEt 退出");
}

int App::run() {
    auto last = std::chrono::steady_clock::now();

    while (!window_->shouldClose()) {
        window_->pollEvents();

        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        input_->beginFrame();

        if (input_->isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window_->handle(), GLFW_TRUE);
        }

        if (input_->isMouseCaptured()) {
            controller_->update(*camera_, *input_, dt);
        }

        input_->endFrame();
        renderer_->drawFrame();
    }

    vk_->waitIdle();
    return 0;
}

}
