#include "app.h"

#include "logger.h"
#include "window.h"
#include "../io/input.h"
#include "../player/player.h"
#include "../player/raycaster.h"
#include "../render/camera.h"
#include "../render/renderer.h"
#include "../render/hud_renderer.h"
#include "../render/swapchain.h"
#include "../render/vulkan_context.h"
#include "../ui/font.h"
#include "../ui/menu.h"
#include "../world/chunk.h"
#include "../world/chunk_manager.h"
#include "../world/particle.h"
#include "../world/terrain_generator.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace nenet {

namespace {

constexpr float kReachDistance = 8.0f;
constexpr int kViewDistance = 4;

constexpr std::array<Block, 6> kHotbarBlocks = {{
    Block::Stone, Block::Dirt, Block::Grass,
    Block::Sand,  Block::Wood, Block::Leaves,
}};

const char* blockName(Block b) {
    switch (b) {
        case Block::Stone:  return "Stone";
        case Block::Dirt:   return "Dirt";
        case Block::Grass:  return "Grass";
        case Block::Sand:   return "Sand";
        case Block::Wood:   return "Wood";
        case Block::Leaves: return "Leaves";
        case Block::Water:  return "Water";
        case Block::Air:
        default:            return "Air";
    }
}

std::array<glm::vec4, 6> hotbarColors() {
    std::array<glm::vec4, 6> out{};
    for (size_t i = 0; i < kHotbarBlocks.size(); ++i) {
        const glm::vec3 c = blockColor(kHotbarBlocks[i]);
        out[i] = glm::vec4(c, 1.0f);
    }
    return out;
}

int floorDivLocal(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

std::string fmt(const char* f, double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
}

std::string fmt(const char* f, int v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
}

}

App::App() {
    log::init();
    spdlog::info("NeNEt 启动");

    window_ = std::make_unique<Window>(1280, 720, "NeNEt");
    vk_ = std::make_unique<VulkanContext>(*window_);
    swapchain_ = std::make_unique<Swapchain>(*vk_, window_->width(), window_->height());

    camera_ = std::make_unique<Camera>();
    camera_->setAspect(static_cast<float>(window_->width()) / static_cast<float>(window_->height()));
    camera_->setPosition({Chunk::kSizeX * 0.5f, 110.0f, Chunk::kSizeZ * 0.5f});
    camera_->setYawPitch(-90.0f, -25.0f);

    generator_ = std::make_unique<TerrainGenerator>(20260430ULL);
    chunkManager_ = std::make_unique<ChunkManager>(vk_->allocator(), *generator_, kViewDistance);
    chunkManager_->update(camera_->position(), std::numeric_limits<int>::max(), false);

    particles_ = std::make_unique<ParticleSystem>(vk_->allocator());
    renderer_ = std::make_unique<Renderer>(*vk_, *swapchain_, *camera_, *chunkManager_, *particles_);

    input_ = std::make_unique<Input>(window_->handle());
    player_ = std::make_unique<Player>(*camera_);
    menu_ = std::make_unique<Menu>();

    enterMenu();

    spdlog::info("控制：WASD 移动 / 鼠标视角 / Ctrl 疾跑 / Space 跳跃 / 双击 Space 切飞行 / ESC 回菜单");
    spdlog::info("交互：左键破坏 / 右键放置 / 1~6 切方块");
    spdlog::info("菜单：左键点 绿色=开始 红色=退出");
}

App::~App() {
    if (vk_) vk_->waitIdle();
    renderer_.reset();
    particles_.reset();
    chunkManager_.reset();
    generator_.reset();
    menu_.reset();
    player_.reset();
    input_.reset();
    camera_.reset();
    swapchain_.reset();
    vk_.reset();
    window_.reset();
    spdlog::info("NeNEt 退出");
}

void App::enterInGame() {
    state_ = State::InGame;
    input_->setMouseCaptured(true);
    spdlog::info("进入游戏");
}

void App::enterMenu() {
    state_ = State::Menu;
    input_->setMouseCaptured(false);
    spdlog::info("回到菜单");
}

void App::handleBlockInteraction() {
    constexpr int kSlotKeys[6] = {
        GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3,
        GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6,
    };
    for (int i = 0; i < 6; ++i) {
        if (input_->isKeyJustPressed(kSlotKeys[i])) {
            selectedSlot_ = i;
            spdlog::info("当前方块：{}", blockName(kHotbarBlocks[selectedSlot_]));
        }
    }
}

void App::tryBreakBlock() {
    auto reader = [this](int x, int y, int z) { return chunkManager_->worldGet(x, y, z); };
    const auto hit = Raycaster::cast(reader, camera_->position(),
                                      camera_->forward(), kReachDistance);
    if (!hit) return;
    const glm::ivec3 pos = hit->blockPos;
    const Block oldBlock = chunkManager_->worldGet(pos.x, pos.y, pos.z);
    chunkManager_->worldSet(pos.x, pos.y, pos.z, Block::Air);
    spdlog::info("破坏 {} ({},{},{})", static_cast<int>(oldBlock), pos.x, pos.y, pos.z);
    if (particles_ && oldBlock != Block::Air) {
        particles_->spawnBlockBreak(pos, oldBlock);
    }
    const int cx = floorDivLocal(pos.x, Chunk::kSizeX);
    const int cz = floorDivLocal(pos.z, Chunk::kSizeZ);
    vk_->waitIdle();
    chunkManager_->rebuildMeshAt(cx, cz);
}

void App::tryPlaceBlock() {
    auto reader = [this](int x, int y, int z) { return chunkManager_->worldGet(x, y, z); };
    const auto hit = Raycaster::cast(reader, camera_->position(),
                                      camera_->forward(), kReachDistance);
    if (!hit) return;
    const glm::ivec3 pos = hit->blockPos + hit->normal;
    const Block b = kHotbarBlocks[selectedSlot_];
    chunkManager_->worldSet(pos.x, pos.y, pos.z, b);
    spdlog::info("放置 {} ({},{},{})", blockName(b), pos.x, pos.y, pos.z);
    const int cx = floorDivLocal(pos.x, Chunk::kSizeX);
    const int cz = floorDivLocal(pos.z, Chunk::kSizeZ);
    vk_->waitIdle();
    chunkManager_->rebuildMeshAt(cx, cz);
}

int App::run() {
    auto last = std::chrono::steady_clock::now();
    const auto colors = hotbarColors();

    while (!window_->shouldClose()) {
        window_->pollEvents();

        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        fpsAccum_ += dt;
        ++fpsFrames_;
        if (fpsAccum_ >= 0.25f) {
            fpsCurrent_ = fpsFrames_ / fpsAccum_;
            fpsAccum_ = 0.0f;
            fpsFrames_ = 0;
        }

        input_->beginFrame();

        if (input_->isKeyJustPressed(GLFW_KEY_F3)) {
            hudVisible_ = !hudVisible_;
            spdlog::info("F3 HUD = {}", hudVisible_);
        }

        if (state_ == State::Menu) {
            const auto action = menu_->update(*input_, static_cast<int>(window_->width()),
                                               static_cast<int>(window_->height()));
            if (action == Menu::Action::StartGame) {
                enterInGame();
            } else if (action == Menu::Action::Quit) {
                glfwSetWindowShouldClose(window_->handle(), GLFW_TRUE);
            }
        } else {
            if (input_->isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                enterMenu();
            } else {
                auto reader = [this](int x, int y, int z) {
                    return chunkManager_->worldGet(x, y, z);
                };

                PlayerInput pin{};
                if (input_->isMouseCaptured()) {
                    pin.lookDelta = input_->mouseDelta();
                    if (input_->isKeyDown(GLFW_KEY_W)) pin.moveAxis.y += 1.0f;
                    if (input_->isKeyDown(GLFW_KEY_S)) pin.moveAxis.y -= 1.0f;
                    if (input_->isKeyDown(GLFW_KEY_A)) pin.moveAxis.x -= 1.0f;
                    if (input_->isKeyDown(GLFW_KEY_D)) pin.moveAxis.x += 1.0f;
                    pin.jumpHeld = input_->isKeyDown(GLFW_KEY_SPACE);
                    pin.jumpJustPressed = input_->isKeyJustPressed(GLFW_KEY_SPACE);
                    pin.flyDown = input_->isKeyDown(GLFW_KEY_LEFT_SHIFT);
                    pin.sprint = input_->isKeyDown(GLFW_KEY_LEFT_CONTROL);

                    if (input_->isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) tryBreakBlock();
                    if (input_->isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_RIGHT)) tryPlaceBlock();
                }

                player_->update(pin, reader, dt);
                handleBlockInteraction();
            }
        }

        input_->endFrame();

        chunkManager_->update(camera_->position(), 1);

        particles_->update(dt);
        particles_->uploadGeometry();

        renderer_->setHotbar(selectedSlot_, colors);
        renderer_->setMenuVisible(state_ == State::Menu, menu_->hoverIndex());

        const bool showHud = (state_ == State::Menu)
                           || (hudVisible_ && state_ == State::InGame);
        renderer_->setHudVisible(showHud);

        if (showHud) {
            auto& hud = renderer_->hud();
            hud.clear();

            if (state_ == State::Menu) {
                const float titlePix = 0.014f;
                const std::string title = "NENET";
                const float titleW = static_cast<float>(title.size()) * kFontAdvance * titlePix;
                hud.addText(title, -titleW * 0.5f, -0.55f, titlePix, glm::vec3(1.0f));

                const float btnPix = 0.006f;
                const float halfTextH = 0.5f * kFontHeight * btnPix;
                const std::string play = "PLAY";
                const std::string quit = "QUIT";
                const float playW = static_cast<float>(play.size()) * kFontAdvance * btnPix;
                const float quitW = static_cast<float>(quit.size()) * kFontAdvance * btnPix;
                hud.addText(play, -playW * 0.5f, 0.00f - halfTextH, btnPix, glm::vec3(1.0f));
                hud.addText(quit, -quitW * 0.5f, 0.18f - halfTextH, btnPix, glm::vec3(1.0f));
            } else {
                const float pix = 0.0035f;
                const glm::vec3 white(1.0f);
                const glm::vec3 yellow(1.0f, 0.95f, 0.4f);
                const glm::vec3 cyan(0.6f, 0.95f, 1.0f);

                const glm::vec3 p = camera_->position();
                hud.addText("FPS " + fmt("%d", static_cast<int>(fpsCurrent_ + 0.5f)),
                            -0.98f, -0.96f, pix, yellow);
                hud.addText("X " + fmt("%.2f", static_cast<double>(p.x)),
                            -0.98f, -0.88f, pix, white);
                hud.addText("Y " + fmt("%.2f", static_cast<double>(p.y)),
                            -0.98f, -0.83f, pix, white);
                hud.addText("Z " + fmt("%.2f", static_cast<double>(p.z)),
                            -0.98f, -0.78f, pix, white);
                hud.addText("YAW " + fmt("%.0f", static_cast<double>(camera_->yaw())),
                            -0.98f, -0.70f, pix, cyan);
                hud.addText("PITCH " + fmt("%.0f", static_cast<double>(camera_->pitch())),
                            -0.98f, -0.65f, pix, cyan);
            }

            hud.upload();
        }

        renderer_->drawFrame();
    }

    vk_->waitIdle();
    return 0;
}

}
