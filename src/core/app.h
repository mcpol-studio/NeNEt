#pragma once

#include "../world/block.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

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
class FallingBlocks;

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    enum class State { Menu, InGame, Paused, Others };

    void handleBlockInteraction();
    void tryBreakBlock();
    void tryPlaceBlock();
    void applySandGravity(int worldX, int worldY, int worldZ);
    void floodWater(int worldX, int worldY, int worldZ);
    void rebuildAroundBlock(int worldX, int worldZ);
    void tryPushPlayerFromFallingBlocks();
    void updatePlayerHealth(float dt);
    void respawnPlayer();
    void renderHealthBar();
    void renderPauseMenu();
    void renderOthersScreen();
    int pauseHitTest(float ndcX, float ndcY) const;
    int othersHitTest(float ndcX, float ndcY) const;
    void launchEmptyDea();
    void fetchAndApplyChunkFromRpc(int cx, int cz);
    void startMirrorThread();
    void stopMirrorThread();
    void drainMirrorQueue();
    void enterInGame();
    void enterMenu();
    void enterPaused();
    void enterOthers();

    bool loadOthersSession();
    void saveOthersSession();

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
    std::unique_ptr<FallingBlocks> falling_;

    State state_{State::Menu};
    int selectedSlot_{0};
    bool hudVisible_{true};

    float fpsAccum_{0.0f};
    int fpsFrames_{0};
    float fpsCurrent_{0.0f};

    float suffocateTimer_{0.0f};
    float regenTimer_{0.0f};
    glm::vec3 spawnPoint_{0.0f};
    bool spawnPointSet_{false};

    int pauseHover_{-1};

    int othersStep_{0};
    int othersHover_{-1};
    int othersActiveField_{-1};
    std::string othersFields_[3];
    std::string othersStatus_;
    std::unique_ptr<class RpcClient> rpc_;
    std::unique_ptr<class UdpClient> udp_;

    struct PendingChunk {
        int cx;
        int cz;
        std::vector<uint8_t> data;
    };
    std::atomic<bool> mirrorRunning_{false};
    std::atomic<bool> mirrorStop_{false};
    std::thread mirrorThread_;
    std::mutex mirrorMu_;
    std::vector<PendingChunk> mirrorPending_;
    std::unordered_map<uint64_t, uint64_t> mirrorHash_;
    int mirrorRange_{2};
    std::atomic<int> mirrorCenterCx_{0};
    std::atomic<int> mirrorCenterCz_{0};
    bool mirrorPlayerTeleported_{false};
    int lastBotMoveCx_{INT32_MIN};
    int lastBotMoveCz_{INT32_MIN};

    bool autoLaunchedOnce_{false};
};

}
