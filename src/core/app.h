#pragma once

#include <memory>

namespace nenet {

class Window;
class VulkanContext;
class Swapchain;
class Camera;
class Renderer;
class Chunk;
class ChunkMesh;
class Input;
class FlyController;

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    std::unique_ptr<Window> window_;
    std::unique_ptr<VulkanContext> vk_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Chunk> chunk_;
    std::unique_ptr<ChunkMesh> chunkMesh_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Input> input_;
    std::unique_ptr<FlyController> controller_;
};

}
