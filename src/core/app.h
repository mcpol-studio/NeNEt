#pragma once

#include "../world/block.h"

#include <memory>

namespace nenet {

class Window;
class VulkanContext;
class Swapchain;
class Camera;
class Renderer;
class ChunkManager;
class TerrainGenerator;
class Input;
class Player;
class Menu;
class ParticleSystem;

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    enum class State { Menu, InGame };

    void handleBlockInteraction();
    void tryBreakBlock();
    void tryPlaceBlock();
    void enterInGame();
    void enterMenu();

    std::unique_ptr<Window> window_;
    std::unique_ptr<VulkanContext> vk_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<TerrainGenerator> generator_;
    std::unique_ptr<ChunkManager> chunkManager_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Input> input_;
    std::unique_ptr<Player> player_;
    std::unique_ptr<Menu> menu_;
    std::unique_ptr<ParticleSystem> particles_;

    State state_{State::Menu};
    int selectedSlot_{0};
    bool hudVisible_{true};

    float fpsAccum_{0.0f};
    int fpsFrames_{0};
    float fpsCurrent_{0.0f};
};

}
