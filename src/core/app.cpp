#include "app.h"

#include "logger.h"
#include "window.h"
#include "../io/input.h"
#include "../io/rpc_client.h"
#include "../io/udp_client.h"
#include "../io/wallpaper.h"
#include "../render/shader.h"
#include "../world/block_registry.h"
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
#include "../world/falling_blocks.h"
#include "../world/particle.h"
#include "../world/terrain_generator.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <utility>
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

    BlockRegistry::instance().load(executableDir() / "blocks" / "atlas_index.txt");
    {
        const auto& reg = BlockRegistry::instance();
        spdlog::info("BlockRegistry sanity: stone={} dirt={} grass_top={} oak_log={} water={} short_grass={}",
                     reg.slotByName("stone"),
                     reg.slotByName("dirt"),
                     reg.slotByName("grass_block_top"),
                     reg.slotByName("oak_log"),
                     reg.slotByName("water_still"),
                     reg.slotByName("short_grass"));
    }

    window_ = std::make_unique<Window>(1280, 720, "NeNEt");
    vk_ = std::make_unique<VulkanContext>(*window_);
    swapchain_ = std::make_unique<Swapchain>(*vk_, window_->width(), window_->height());

    camera_ = std::make_unique<Camera>();
    camera_->setAspect(static_cast<float>(window_->width()) / static_cast<float>(window_->height()));
    camera_->setPosition({Chunk::kSizeX * 0.5f, 110.0f, Chunk::kSizeZ * 0.5f});
    camera_->setYawPitch(-90.0f, -25.0f);

    generator_ = std::make_unique<TerrainGenerator>(20260430ULL);
    chunkManager_ = std::make_unique<ChunkManager>(vk_->allocator(), *generator_, kViewDistance);

    particles_ = std::make_unique<ParticleSystem>(vk_->allocator());
    falling_ = std::make_unique<FallingBlocks>(vk_->allocator());

    const auto wallpaperCache = executableDir() / "cache" / "wallpaper.jpg";
    auto wallpaperPath = ensureWallpaper(wallpaperCache);

    renderer_ = std::make_unique<Renderer>(*vk_, *swapchain_, *camera_, *chunkManager_,
                                            *particles_, *falling_, wallpaperPath);

    input_ = std::make_unique<Input>(window_->handle());
    player_ = std::make_unique<Player>(*camera_);
    menu_ = std::make_unique<Menu>();

    enterMenu();

    spdlog::info("控制：WASD 移动 / 鼠标视角 / Ctrl 疾跑 / Space 跳跃 / 双击 Space 切飞行 / ESC 回菜单");
    spdlog::info("交互：左键破坏 / 右键放置 / 1~6 切方块");
    spdlog::info("菜单：左键点 绿色=开始 红色=退出");

    if (!autoLaunchedOnce_ && loadOthersSession()) {
        autoLaunchedOnce_ = true;
        spdlog::info("Others 会话已恢复，自动启动 EmptyDea");
        enterOthers();
        launchEmptyDea();
    }
}

App::~App() {
    stopMirrorThread();
    if (vk_) vk_->waitIdle();
    renderer_.reset();
    falling_.reset();
    particles_.reset();
    chunkManager_.reset();
    generator_.reset();
    menu_.reset();
    player_.reset();
    input_.reset();
    camera_.reset();
    swapchain_.reset();
    vk_.reset();
    udp_.reset();
    rpc_.reset();
    window_.reset();
    spdlog::info("NeNEt 退出");
}

void App::enterInGame() {
    state_ = State::InGame;
    input_->setMouseCaptured(true);
    if (!spawnPointSet_ && player_) {
        spawnPoint_ = player_->footPos();
        spawnPointSet_ = true;
    }
    spdlog::info("进入游戏");
}

void App::enterMenu() {
    stopMirrorThread();
    state_ = State::Menu;
    input_->setMouseCaptured(false);
    spdlog::info("回到菜单");
}

void App::enterPaused() {
    state_ = State::Paused;
    input_->setMouseCaptured(false);
    pauseHover_ = -1;
    spdlog::info("游戏暂停");
}

void App::enterOthers() {
    state_ = State::Others;
    input_->setMouseCaptured(false);
    othersStep_ = 0;
    othersHover_ = -1;
    othersActiveField_ = -1;
    othersStatus_.clear();

    if (chunkManager_) chunkManager_->setEmptyMode(true);
    spdlog::info("进入 Others 工具（empty mode 已开启）");
}

namespace {

struct OthersRect {
    float x, y, w, h;
};

constexpr OthersRect kOthersStep0Buttons[2] = {
    {-0.30f,  0.30f, 0.28f, 0.10f},
    { 0.02f,  0.30f, 0.28f, 0.10f},
};

constexpr OthersRect kOthersFieldRects[3] = {
    {-0.45f, -0.28f, 0.90f, 0.08f},
    {-0.45f, -0.13f, 0.90f, 0.08f},
    {-0.45f,  0.02f, 0.90f, 0.08f},
};

constexpr OthersRect kOthersStep1Buttons[2] = {
    {-0.45f, 0.32f, 0.42f, 0.10f},
    { 0.03f, 0.32f, 0.42f, 0.10f},
};

constexpr OthersRect kOthersStep2Buttons[2] = {
    {-0.45f, 0.40f, 0.42f, 0.10f},
    { 0.03f, 0.40f, 0.42f, 0.10f},
};

constexpr const char* kOthersFieldLabels[3] = {
    "SERVER CODE", "SERVER PASSWORD (OPTIONAL)", "COOKIE / TOKEN",
};

bool insideRect(const OthersRect& r, float ndcX, float ndcY) {
    return ndcX >= r.x && ndcX <= r.x + r.w &&
           ndcY >= r.y && ndcY <= r.y + r.h;
}

}

int App::othersHitTest(float ndcX, float ndcY) const {
    if (othersStep_ == 0) {
        if (insideRect(kOthersStep0Buttons[0], ndcX, ndcY)) return 0;
        if (insideRect(kOthersStep0Buttons[1], ndcX, ndcY)) return 1;
        return -1;
    }
    if (othersStep_ == 2) {
        if (insideRect(kOthersStep2Buttons[0], ndcX, ndcY)) return 0;
        if (insideRect(kOthersStep2Buttons[1], ndcX, ndcY)) return 1;
        return -1;
    }
    for (int i = 0; i < 3; ++i) {
        if (insideRect(kOthersFieldRects[i], ndcX, ndcY)) return 100 + i;
    }
    if (insideRect(kOthersStep1Buttons[0], ndcX, ndcY)) return 0;
    if (insideRect(kOthersStep1Buttons[1], ndcX, ndcY)) return 1;
    return -1;
}

void App::launchEmptyDea() {
    const std::filesystem::path moduleDir = std::filesystem::current_path() / "modules" / "EmptyDea";
    const std::filesystem::path exe = moduleDir / "EmptyDea.exe";
    std::error_code ec;
    if (!std::filesystem::exists(exe, ec)) {
        othersStatus_ = "EmptyDea.exe not found. Build it in modules/EmptyDea first.";
        spdlog::warn("EmptyDea.exe 不存在: {}", exe.string());
        return;
    }
    auto sanitizeArg = [](const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '"' || c == '\\' || c == '\r' || c == '\n') continue;
            out.push_back(c);
        }
        return out;
    };
    const std::string code       = sanitizeArg(othersFields_[0]);
    const std::string serverPass = sanitizeArg(othersFields_[1]);
    const std::string& cookie    = othersFields_[2];
    const std::string rpcAddr    = "127.0.0.1:24050";

    std::filesystem::path cookieFile;
    if (!cookie.empty()) {
        cookieFile = moduleDir / ".cookie.json";
        std::ofstream ofs(cookieFile, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            othersStatus_ = "Failed to write cookie file";
            return;
        }
        ofs.write(cookie.data(), static_cast<std::streamsize>(cookie.size()));
        ofs.close();
        spdlog::info("Cookie 已写入 {} ({} 字节)", cookieFile.string(), cookie.size());
    }

    std::string cmd = "cmd.exe /c start \"EmptyDea\" /D \"";
    cmd += moduleDir.string();
    cmd += "\" \"EmptyDea.exe\"";
    cmd += " --http-rpc-addr " + rpcAddr;
    if (!code.empty())       cmd += " --server \"" + code + "\"";
    if (!serverPass.empty()) cmd += " --server-password \"" + serverPass + "\"";
    if (!cookieFile.empty()) cmd += " --token-file \"" + cookieFile.string() + "\"";
    spdlog::info("启动 EmptyDea: {}", cmd);
    const int rc = std::system(cmd.c_str());
    if (rc == 0) {
        othersStatus_ = "Launched. Connecting to RPC " + rpcAddr + " ...";
        if (!rpc_) rpc_ = std::make_unique<RpcClient>("http://" + rpcAddr);
        else rpc_->setBaseUrl("http://" + rpcAddr);
        othersStep_ = 2;

        if (vk_) vk_->waitIdle();
        if (chunkManager_) chunkManager_->setEmptyMode(true);
        mirrorPlayerTeleported_ = false;

#ifndef NENET_DISABLE_UDP_MIRROR
        try {
            udp_ = std::make_unique<UdpClient>();
            if (!udp_->start("127.0.0.1:24051")) {
                spdlog::warn("UdpClient: start 失败，mirror 不会收到 push");
            }
        } catch (const std::exception& e) {
            spdlog::error("UdpClient 构造异常: {}", e.what());
            udp_.reset();
        }
#endif
        startMirrorThread();
        othersStatus_ = "Mirror running 5x5 chunks @1Hz (follow player)";

        saveOthersSession();
        enterInGame();
    } else {
        othersStatus_ = "Launch failed (exit " + std::to_string(rc) + ")";
    }
}

namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

std::filesystem::path othersSessionPath() {
    return executableDir() / "cache" / "others_session.json";
}

std::string buildOthersSessionJson(const std::string& server,
                                    const std::string& password,
                                    const std::string& cookie) {
    std::string out;
    out += "{\"server\":\"";
    out += jsonEscape(server);
    out += "\",\"password\":\"";
    out += jsonEscape(password);
    out += "\",\"cookie\":\"";
    out += jsonEscape(cookie);
    out += "\"}";
    return out;
}

}

void App::saveOthersSession() {
    const std::string content = buildOthersSessionJson(
        othersFields_[0], othersFields_[1], othersFields_[2]);
    const auto path = othersSessionPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    {
        std::ifstream ifs(path, std::ios::binary);
        if (ifs) {
            std::string old((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
            if (old == content) {
                spdlog::info("Others 会话未变，跳过写入");
                return;
            }
        }
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        spdlog::warn("无法写入 others 会话文件: {}", path.string());
        return;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.close();
    spdlog::info("Others 会话已保存: {} ({} bytes)", path.string(), content.size());
}

void App::startMirrorThread() {
    if (mirrorRunning_.load()) return;
    mirrorStop_.store(false);
    mirrorRunning_.store(true);

    mirrorThread_ = std::thread([this]{
        while (!mirrorStop_.load()) {
            if (udp_) {
                auto chunks = udp_->drainCompleted();
                if (!chunks.empty()) {
                    std::lock_guard<std::mutex> lk(mirrorMu_);
                    for (auto& c : chunks) {
                        constexpr size_t kExpect = 16 * 256 * 16;
                        if (c.data.size() != kExpect) continue;

                        uint64_t h = 1469598103934665603ULL;
                        for (uint8_t b : c.data) {
                            h ^= b;
                            h *= 1099511628211ULL;
                        }
                        const uint64_t key =
                            (static_cast<uint64_t>(static_cast<uint32_t>(c.cx)) << 32) |
                             static_cast<uint64_t>(static_cast<uint32_t>(c.cz));
                        auto it = mirrorHash_.find(key);
                        if (it != mirrorHash_.end() && it->second == h) continue;
                        mirrorHash_[key] = h;
                        mirrorPending_.push_back({c.cx, c.cz, std::move(c.data)});
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
}

void App::stopMirrorThread() {
    if (!mirrorRunning_.load()) {

        if (udp_) {
            udp_->stop();
            udp_.reset();
        }
        return;
    }
    mirrorStop_.store(true);

    if (udp_) {
        udp_->stop();
    }
    if (mirrorThread_.joinable()) mirrorThread_.join();
    mirrorRunning_.store(false);
    {
        std::lock_guard<std::mutex> lk(mirrorMu_);
        mirrorPending_.clear();
        mirrorHash_.clear();
    }
    udp_.reset();
}

void App::drainMirrorQueue() {
    std::vector<PendingChunk> batch;
    {
        std::lock_guard<std::mutex> lk(mirrorMu_);

        const size_t n = std::min<size_t>(4, mirrorPending_.size());
        if (n == 0) return;
        batch.insert(batch.end(),
                     std::make_move_iterator(mirrorPending_.begin()),
                     std::make_move_iterator(mirrorPending_.begin() + n));
        mirrorPending_.erase(mirrorPending_.begin(), mirrorPending_.begin() + n);
    }
    if (!chunkManager_) return;
    if (vk_) vk_->waitIdle();
    std::set<std::pair<int,int>> dirtyChunks;
    for (const auto& pc : batch) {

        chunkManager_->ensureChunk(pc.cx, pc.cz);
        for (int y = 0; y < 256; ++y) {
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    const uint8_t id = pc.data[(y * 16 + z) * 16 + x];
                    const Block b = (id <= static_cast<uint8_t>(Block::Cactus))
                        ? static_cast<Block>(id) : Block::Stone;
                    chunkManager_->worldSet(pc.cx * 16 + x, y, pc.cz * 16 + z, b);
                }
            }
        }
        dirtyChunks.insert({pc.cx, pc.cz});
        dirtyChunks.insert({pc.cx - 1, pc.cz});
        dirtyChunks.insert({pc.cx + 1, pc.cz});
        dirtyChunks.insert({pc.cx, pc.cz - 1});
        dirtyChunks.insert({pc.cx, pc.cz + 1});

        if (!mirrorPlayerTeleported_ && player_) {
            int topY = -1;
            for (int y = 255; y >= 0 && topY < 0; --y) {
                for (int z = 0; z < 16 && topY < 0; ++z) {
                    for (int x = 0; x < 16 && topY < 0; ++x) {
                        if (pc.data[(y * 16 + z) * 16 + x] != static_cast<uint8_t>(Block::Air)) {
                            topY = y;
                        }
                    }
                }
            }
            if (topY >= 0) {
                const int worldX = pc.cx * 16 + 8;
                const int worldZ = pc.cz * 16 + 8;
                int landY = topY;
                while (landY > 0 && chunkManager_->worldGet(worldX, landY, worldZ) == Block::Air) {
                    --landY;
                }
                player_->setFootPos(glm::vec3(static_cast<float>(worldX) + 0.5f,
                                               static_cast<float>(landY + 2),
                                               static_cast<float>(worldZ) + 0.5f));
                mirrorCenterCx_.store(pc.cx);
                mirrorCenterCz_.store(pc.cz);
                mirrorPlayerTeleported_ = true;
                spdlog::info("玩家瞬移到 mirror 首块非空 chunk ({},{}) 顶面 y={}", pc.cx, pc.cz, landY + 2);
            } else {
                spdlog::info("mirror chunk ({},{}) 全 Air，等待下一个有地形的 chunk", pc.cx, pc.cz);
            }
        }
    }
    for (const auto& cc : dirtyChunks) {

        chunkManager_->rebuildMeshAt(cc.first, cc.second);
    }
    spdlog::info("drainMirrorQueue 应用了 {} 个 chunk，rebuild {} mesh，pending 队列剩 {}",
                 batch.size(), dirtyChunks.size(),
                 [&]{ std::lock_guard<std::mutex> lk(mirrorMu_); return mirrorPending_.size(); }());
}

namespace {

std::string extractJsonString(const std::string& body, const std::string& key) {
    const std::string pat = "\"" + key + "\"";
    auto p = body.find(pat);
    if (p == std::string::npos) return "";
    p = body.find(':', p);
    if (p == std::string::npos) return "";
    ++p;
    while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) ++p;
    if (p >= body.size()) return "";
    if (body[p] == '"') {
        ++p;
        std::string out;
        while (p < body.size() && body[p] != '"') {
            if (body[p] == '\\' && p + 1 < body.size()) ++p;
            out.push_back(body[p++]);
        }
        return out;
    }
    std::string out;
    while (p < body.size() && body[p] != ',' && body[p] != '}' && body[p] != '\n') {
        out.push_back(body[p++]);
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
    return out;
}

}

bool App::loadOthersSession() {
    const auto path = othersSessionPath();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    std::string body((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
    if (body.empty()) return false;

    const std::string server   = extractJsonString(body, "server");
    const std::string password = extractJsonString(body, "password");
    const std::string cookie   = extractJsonString(body, "cookie");

    if (server.empty() || password.empty() || cookie.empty()) {
        spdlog::info("Others 会话存在但字段不完整，不自动启动");
        return false;
    }

    othersFields_[0] = server;
    othersFields_[1] = password;
    othersFields_[2] = cookie;
    return true;
}

void App::fetchAndApplyChunkFromRpc(int cx, int cz) {
    if (!rpc_) {
        othersStatus_ = "RPC not connected";
        return;
    }
    char path[64];
    std::snprintf(path, sizeof(path), "/chunk?cx=%d&cz=%d", cx, cz);
    auto data = rpc_->fetchBinary(path, 5000);
    constexpr size_t kExpected = 16 * 256 * 16;
    if (data.size() != kExpected) {
        othersStatus_ = "Bad chunk size " + std::to_string(data.size()) + " (need " +
                         std::to_string(kExpected) + ")";
        spdlog::warn("/chunk 返回 {} 字节，预期 {}", data.size(), kExpected);
        return;
    }

    if (vk_) vk_->waitIdle();
    for (int y = 0; y < 256; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                const uint8_t id = data[(y * 16 + z) * 16 + x];
                const Block b = (id <= static_cast<uint8_t>(Block::Cactus))
                    ? static_cast<Block>(id) : Block::Stone;
                chunkManager_->worldSet(cx * 16 + x, y, cz * 16 + z, b);
            }
        }
    }
    rebuildAroundBlock(cx * 16, cz * 16);

    if (player_) {
        glm::vec3 foot = player_->footPos();
        const int worldX = cx * 16 + 8;
        const int worldZ = cz * 16 + 8;
        int landY = 255;
        while (landY > 0 &&
               chunkManager_->worldGet(worldX, landY, worldZ) == Block::Air) {
            --landY;
        }
        foot = glm::vec3(static_cast<float>(worldX) + 0.5f,
                          static_cast<float>(landY + 1),
                          static_cast<float>(worldZ) + 0.5f);
        player_->setFootPos(foot);
    }

    othersStatus_ = "Chunk (" + std::to_string(cx) + "," + std::to_string(cz) +
                    ") loaded. Press ESC to view.";
    spdlog::info("已应用 RPC chunk ({}, {})", cx, cz);
}

void App::renderOthersScreen() {
    auto& hud = renderer_->hud();

    const glm::vec3 panelDark(0.07f, 0.09f, 0.14f);
    const glm::vec3 panelEdge(0.18f, 0.24f, 0.34f);
    const glm::vec3 white(1.0f);
    const glm::vec3 dim(0.65f, 0.70f, 0.80f);
    const glm::vec3 warn(1.00f, 0.78f, 0.20f);
    const glm::vec3 good(0.40f, 0.92f, 0.55f);
    const glm::vec3 fieldBg(0.10f, 0.13f, 0.20f);
    const glm::vec3 fieldEdge(0.32f, 0.42f, 0.62f);
    const glm::vec3 fieldEdgeActive(1.00f, 0.85f, 0.30f);

    hud.addRect(-0.55f, -0.78f, 0.55f, 0.55f, panelDark);
    hud.addRect(-0.55f, -0.78f, 0.55f, -0.76f, panelEdge);
    hud.addRect(-0.55f,  0.53f, 0.55f, 0.55f, panelEdge);
    hud.addRect(-0.55f, -0.78f,-0.53f, 0.55f, panelEdge);
    hud.addRect( 0.53f, -0.78f, 0.55f, 0.55f, panelEdge);

    const float titlePix = 0.012f;
    const std::string titleStr = (othersStep_ == 2) ? "EMPTYDEA RPC" : "OTHERS";
    const float titleW = static_cast<float>(titleStr.size()) * kFontAdvance * titlePix;
    hud.addText(titleStr, -titleW * 0.5f, -0.65f, titlePix, white);

    if (othersStep_ == 0) {
        const float msgPix = 0.0070f;
        const std::string msg = "THIS IS BETA TOOL";
        const float msgW = static_cast<float>(msg.size()) * kFontAdvance * msgPix;
        hud.addText(msg, -msgW * 0.5f, -0.20f, msgPix, warn);

        const float subPix = 0.0040f;
        const std::string sub = "PROCEED ONLY IF YOU UNDERSTAND THE RISK";
        const float subW = static_cast<float>(sub.size()) * kFontAdvance * subPix;
        hud.addText(sub, -subW * 0.5f, -0.05f, subPix, dim);

        const char* labels[2] = {"CONTINUE", "BACK"};
        const glm::vec3 baseCol[2] = {{0.30f, 0.65f, 0.30f}, {0.50f, 0.50f, 0.50f}};
        const float btnPix = 0.0060f;
        const float btnH = kFontHeight * btnPix;
        for (int i = 0; i < 2; ++i) {
            const auto& r = kOthersStep0Buttons[i];
            const bool hover = (othersHover_ == i);
            glm::vec3 fill = hover ? glm::mix(baseCol[i], white, 0.30f) : baseCol[i];
            hud.addRect(r.x, r.y, r.x + r.w, r.y + r.h, fill);
            const std::string lbl = labels[i];
            const float lblW = static_cast<float>(lbl.size()) * kFontAdvance * btnPix;
            const float cx = r.x + r.w * 0.5f;
            const float cy = r.y + r.h * 0.5f;
            hud.addText(lbl, cx - lblW * 0.5f, cy - btnH * 0.5f, btnPix, white);
        }
        return;
    }

    if (othersStep_ == 2) {
        const std::string statusBody  = rpc_ ? rpc_->statusBody()  : std::string();
        const std::string versionBody = rpc_ ? rpc_->versionBody() : std::string();
        const bool reachable = rpc_ && rpc_->isReachable();

        const float linePix = 0.0040f;
        const float lineH = kFontHeight * linePix * 1.6f;
        float y = -0.45f;

        auto drawKv = [&](const std::string& k, const std::string& v, const glm::vec3& vc) {
            hud.addText(k, -0.45f, y, linePix, dim);
            hud.addText(v.empty() ? std::string("--") : v, -0.05f, y, linePix, vc);
            y += lineH;
        };

        drawKv("CONNECTION", reachable ? "ONLINE" : "WAITING...", reachable ? good : warn);
        drawKv("ENDPOINT",   rpc_ ? rpc_->baseUrl() : std::string("--"), white);
        drawKv("VERSION",    extractJsonString(versionBody, "version"), white);
        drawKv("UPTIME(s)",  extractJsonString(statusBody,  "uptimeSec"), white);
        drawKv("USER",       extractJsonString(statusBody,  "user"),     white);
        drawKv("SERVER",     extractJsonString(statusBody,  "server"),   white);
        drawKv("READY",      extractJsonString(statusBody,  "ready"),    white);

        const char* labels2[2] = {"FETCH CHUNK", "BACK"};
        const glm::vec3 baseCol2[2] = {{0.30f, 0.55f, 0.85f}, {0.50f, 0.50f, 0.50f}};
        const float btnPix = 0.0055f;
        const float btnH = kFontHeight * btnPix;
        for (int i = 0; i < 2; ++i) {
            const auto& r = kOthersStep2Buttons[i];
            const bool hover = (othersHover_ == i);
            glm::vec3 fill = hover ? glm::mix(baseCol2[i], white, 0.30f) : baseCol2[i];
            hud.addRect(r.x, r.y, r.x + r.w, r.y + r.h, fill);
            const std::string lbl = labels2[i];
            const float lblW = static_cast<float>(lbl.size()) * kFontAdvance * btnPix;
            const float cx = r.x + r.w * 0.5f;
            const float cy = r.y + r.h * 0.5f;
            hud.addText(lbl, cx - lblW * 0.5f, cy - btnH * 0.5f, btnPix, white);
        }
        if (!othersStatus_.empty()) {
            const float sPix = 0.0035f;
            const float sW = static_cast<float>(othersStatus_.size()) * kFontAdvance * sPix;
            hud.addText(othersStatus_, -sW * 0.5f, 0.30f, sPix, warn);
        }
        return;
    }

    const float lblPix = 0.0035f;
    const float fieldTextPix = 0.0042f;
    const float fieldTextH = kFontHeight * fieldTextPix;
    const float aspect = window_ ? (static_cast<float>(window_->width()) /
                                      static_cast<float>(window_->height()))
                                  : 1.78f;
    for (int i = 0; i < 3; ++i) {
        const auto& r = kOthersFieldRects[i];
        const bool active = (othersActiveField_ == i);
        const glm::vec3 edgeCol = active ? fieldEdgeActive : fieldEdge;
        hud.addRect(r.x, r.y, r.x + r.w, r.y + r.h, fieldBg);
        const float t = 0.004f;
        hud.addRect(r.x, r.y, r.x + r.w, r.y + t, edgeCol);
        hud.addRect(r.x, r.y + r.h - t, r.x + r.w, r.y + r.h, edgeCol);
        hud.addRect(r.x, r.y, r.x + t / 2.0f, r.y + r.h, edgeCol);
        hud.addRect(r.x + r.w - t / 2.0f, r.y, r.x + r.w, r.y + r.h, edgeCol);

        const std::string lbl = kOthersFieldLabels[i];
        hud.addText(lbl, r.x, r.y - 0.045f, lblPix, dim);

        std::string display = othersFields_[i];
        if (i == 1) display.assign(othersFields_[i].size(), '*');
        if (active) display.push_back('_');

        const float charNdcX = kFontAdvance * fieldTextPix;
        const float pad = 0.012f;
        const float innerW = r.w - 2.0f * pad;
        const size_t maxChars = (charNdcX > 0.0f)
            ? static_cast<size_t>(innerW / charNdcX * aspect)
            : display.size();
        if (display.size() > maxChars && maxChars > 1) {
            display = display.substr(display.size() - maxChars);
        }
        hud.addText(display, r.x + pad,
                     r.y + r.h * 0.5f - fieldTextH * 0.5f, fieldTextPix, white);

        if (i == 2 && othersFields_[i].size() > 20) {
            const std::string lenStr = "len=" + std::to_string(othersFields_[i].size());
            const float lpW = static_cast<float>(lenStr.size()) * kFontAdvance * lblPix;
            hud.addText(lenStr, r.x + r.w - lpW, r.y - 0.045f, lblPix, dim);
        }
    }

    const char* labels[2] = {"LAUNCH", "BACK"};
    const glm::vec3 baseCol[2] = {{0.30f, 0.65f, 0.30f}, {0.50f, 0.50f, 0.50f}};
    const float btnPix = 0.0060f;
    const float btnH = kFontHeight * btnPix;
    for (int i = 0; i < 2; ++i) {
        const auto& r = kOthersStep1Buttons[i];
        const bool hover = (othersHover_ == i);
        glm::vec3 fill = hover ? glm::mix(baseCol[i], white, 0.30f) : baseCol[i];
        hud.addRect(r.x, r.y, r.x + r.w, r.y + r.h, fill);
        const std::string lbl = labels[i];
        const float lblW = static_cast<float>(lbl.size()) * kFontAdvance * btnPix;
        const float cx = r.x + r.w * 0.5f;
        const float cy = r.y + r.h * 0.5f;
        hud.addText(lbl, cx - lblW * 0.5f, cy - btnH * 0.5f, btnPix, white);
    }

    if (!othersStatus_.empty()) {
        const float sPix = 0.0035f;
        const float sW = static_cast<float>(othersStatus_.size()) * kFontAdvance * sPix;
        hud.addText(othersStatus_, -sW * 0.5f, 0.45f, sPix, warn);
    }
}

namespace {

struct PauseRect {
    float x, y, w, h;
};

constexpr PauseRect kPauseButtons[3] = {
    {-0.30f, -0.12f, 0.60f, 0.10f},
    {-0.30f,  0.02f, 0.60f, 0.10f},
    {-0.30f,  0.16f, 0.60f, 0.10f},
};

}

int App::pauseHitTest(float ndcX, float ndcY) const {
    for (int i = 0; i < 3; ++i) {
        const auto& r = kPauseButtons[i];
        if (ndcX >= r.x && ndcX <= r.x + r.w &&
            ndcY >= r.y && ndcY <= r.y + r.h) {
            return i;
        }
    }
    return -1;
}

void App::renderPauseMenu() {
    if (!renderer_) return;
    auto& hud = renderer_->hud();

    const glm::vec3 dimOverlay(0.04f, 0.05f, 0.08f);
    const glm::vec3 panelEdge(0.18f, 0.24f, 0.34f);
    const glm::vec3 accent(0.42f, 0.78f, 0.96f);
    const glm::vec3 white(1.0f);
    const glm::vec3 dim(0.65f, 0.70f, 0.80f);

    hud.addRect(-1.0f, -1.0f, 1.0f, 1.0f, dimOverlay);
    hud.addRect(-0.40f, -0.55f, 0.40f, 0.40f, glm::vec3(0.07f, 0.09f, 0.14f));
    hud.addRect(-0.40f, -0.55f, 0.40f, -0.535f, panelEdge);
    hud.addRect(-0.40f,  0.385f, 0.40f, 0.40f, panelEdge);

    const float titleRibY0 = -0.45f;
    const float titleRibY1 = -0.30f;
    hud.addRect(-0.36f, titleRibY0, 0.36f, titleRibY1, accent);
    const float titlePix = 0.012f;
    const std::string title = "GAME PAUSED";
    const float titleW = static_cast<float>(title.size()) * kFontAdvance * titlePix;
    const float titleH = kFontHeight * titlePix;
    const float titleY = (titleRibY0 + titleRibY1) * 0.5f - titleH * 0.5f;
    hud.addText(title, -titleW * 0.5f, titleY, titlePix, glm::vec3(0.04f, 0.06f, 0.10f));

    const char* labels[3] = {"CONTINUE", "MAIN MENU", "QUIT"};
    const glm::vec3 baseCol[3] = {
        {0.30f, 0.65f, 0.30f}, {0.45f, 0.55f, 0.85f}, {0.65f, 0.25f, 0.25f},
    };
    const float btnPix = 0.0070f;
    const float btnTextH = kFontHeight * btnPix;
    for (int i = 0; i < 3; ++i) {
        const auto& r = kPauseButtons[i];
        const bool hover = (pauseHover_ == i);
        glm::vec3 fill = hover ? glm::mix(baseCol[i], white, 0.30f) : baseCol[i];
        hud.addRect(r.x, r.y, r.x + r.w, r.y + r.h, fill);
        const std::string lbl = labels[i];
        const float lblW = static_cast<float>(lbl.size()) * kFontAdvance * btnPix;
        const float cx = r.x + r.w * 0.5f;
        const float cy = r.y + r.h * 0.5f;
        hud.addText(lbl, cx - lblW * 0.5f, cy - btnTextH * 0.5f, btnPix, white);
    }

    const float hintPix = 0.0035f;
    const std::string hint = "ESC TO RESUME";
    const float hintW = static_cast<float>(hint.size()) * kFontAdvance * hintPix;
    hud.addText(hint, -hintW * 0.5f, 0.32f, hintPix, dim);
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

    const float scroll = input_->scrollDelta();
    if (scroll != 0.0f) {
        const int dir = scroll > 0.0f ? -1 : 1;
        selectedSlot_ = ((selectedSlot_ + dir) % 6 + 6) % 6;
        spdlog::info("当前方块（滚轮）：{}", blockName(kHotbarBlocks[selectedSlot_]));
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

    int upY = pos.y + 1;
    while (upY < Chunk::kSizeY) {
        const Block above = chunkManager_->worldGet(pos.x, upY, pos.z);
        if (!isCrossPlant(above)) break;
        chunkManager_->worldSet(pos.x, upY, pos.z, Block::Air);
        if (particles_) {
            particles_->spawnBlockBreak({pos.x, upY, pos.z}, above);
        }
        ++upY;
    }

    applySandGravity(pos.x, pos.y, pos.z);
    floodWater(pos.x, pos.y, pos.z);
    rebuildAroundBlock(pos.x, pos.z);
}

void App::applySandGravity(int worldX, int worldY, int worldZ) {
    int probe = worldY + 1;
    while (probe < Chunk::kSizeY) {
        const Block b = chunkManager_->worldGet(worldX, probe, worldZ);
        if (!hasGravity(b)) break;
        const Block below = chunkManager_->worldGet(worldX, probe - 1, worldZ);
        const bool airBelow = (below == Block::Air);
        if (airBelow) {
            chunkManager_->worldSet(worldX, probe, worldZ, Block::Air);
            if (falling_) {
                falling_->spawn(glm::vec3(static_cast<float>(worldX),
                                           static_cast<float>(probe),
                                           static_cast<float>(worldZ)), b);
            }
        }
        ++probe;
    }
}

void App::floodWater(int worldX, int worldY, int worldZ) {
    std::queue<glm::ivec3> q;
    q.push({worldX, worldY, worldZ});
    int budget = 256;
    while (!q.empty() && budget-- > 0) {
        const glm::ivec3 p = q.front();
        q.pop();
        if (chunkManager_->worldGet(p.x, p.y, p.z) != Block::Air) continue;

        const Block above = chunkManager_->worldGet(p.x, p.y + 1, p.z);
        const Block below = chunkManager_->worldGet(p.x, p.y - 1, p.z);
        const Block west  = chunkManager_->worldGet(p.x - 1, p.y, p.z);
        const Block east  = chunkManager_->worldGet(p.x + 1, p.y, p.z);
        const Block north = chunkManager_->worldGet(p.x, p.y, p.z - 1);
        const Block south = chunkManager_->worldGet(p.x, p.y, p.z + 1);

        bool fill = false;
        if (above == Block::Water) {
            fill = true;
        } else if (below != Block::Air && below != Block::Water) {
            if (west == Block::Water || east == Block::Water ||
                north == Block::Water || south == Block::Water) {
                fill = true;
            }
        }

        if (!fill) continue;
        chunkManager_->worldSet(p.x, p.y, p.z, Block::Water);
        q.push({p.x, p.y - 1, p.z});
        q.push({p.x - 1, p.y, p.z});
        q.push({p.x + 1, p.y, p.z});
        q.push({p.x, p.y, p.z - 1});
        q.push({p.x, p.y, p.z + 1});
    }
}

void App::rebuildAroundBlock(int worldX, int worldZ) {
    const int cx = floorDivLocal(worldX, Chunk::kSizeX);
    const int cz = floorDivLocal(worldZ, Chunk::kSizeZ);
    vk_->waitIdle();
    chunkManager_->rebuildMeshAt(cx, cz);
    chunkManager_->rebuildMeshAt(cx - 1, cz);
    chunkManager_->rebuildMeshAt(cx + 1, cz);
    chunkManager_->rebuildMeshAt(cx, cz - 1);
    chunkManager_->rebuildMeshAt(cx, cz + 1);
}

void App::tryPushPlayerFromFallingBlocks() {
    if (!falling_ || !player_) return;
    constexpr float kHalfWidth = 0.30f;
    constexpr float kHeight = 1.80f;
    auto reader = [this](int x, int y, int z) { return chunkManager_->worldGet(x, y, z); };

    auto isSolid = [](Block b) {
        return b != Block::Air && b != Block::Water && b != Block::Lava
            && !isCrossPlant(b);
    };

    auto fitsAt = [&](const glm::vec3& foot) -> bool {
        const int x0 = static_cast<int>(std::floor(foot.x - kHalfWidth));
        const int x1 = static_cast<int>(std::floor(foot.x + kHalfWidth - 1e-4f));
        const int y0 = static_cast<int>(std::floor(foot.y));
        const int y1 = static_cast<int>(std::floor(foot.y + kHeight - 1e-4f));
        const int z0 = static_cast<int>(std::floor(foot.z - kHalfWidth));
        const int z1 = static_cast<int>(std::floor(foot.z + kHalfWidth - 1e-4f));
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                for (int x = x0; x <= x1; ++x) {
                    if (isSolid(reader(x, y, z))) return false;
                }
            }
        }
        return true;
    };

    for (const auto& e : falling_->entities()) {
        const glm::vec3 foot = player_->footPos();
        const glm::vec3 pmin{foot.x - kHalfWidth, foot.y, foot.z - kHalfWidth};
        const glm::vec3 pmax{foot.x + kHalfWidth, foot.y + kHeight, foot.z + kHalfWidth};
        const glm::vec3 bmin = e.position;
        const glm::vec3 bmax = e.position + glm::vec3(1.0f);
        const bool overlap = (pmin.x < bmax.x && pmax.x > bmin.x) &&
                             (pmin.y < bmax.y && pmax.y > bmin.y) &&
                             (pmin.z < bmax.z && pmax.z > bmin.z);
        if (!overlap) continue;

        const glm::vec3 candidates[4] = {
            foot + glm::vec3(+1.0f, 0.0f, 0.0f),
            foot + glm::vec3(-1.0f, 0.0f, 0.0f),
            foot + glm::vec3(0.0f, 0.0f, +1.0f),
            foot + glm::vec3(0.0f, 0.0f, -1.0f),
        };
        for (const auto& c : candidates) {
            if (fitsAt(c)) {
                player_->setFootPos(c);
                break;
            }
        }
    }
}

void App::updatePlayerHealth(float dt) {
    if (!player_) return;

    const glm::vec3 eye = player_->eyePos();
    const int ex = static_cast<int>(std::floor(eye.x));
    const int ey = static_cast<int>(std::floor(eye.y));
    const int ez = static_cast<int>(std::floor(eye.z));
    const Block bk = chunkManager_->worldGet(ex, ey, ez);
    const bool inSolid = (bk != Block::Air && bk != Block::Water && bk != Block::Lava
                          && !isCrossPlant(bk));

    if (inSolid) {
        suffocateTimer_ += dt;
        regenTimer_ = 0.0f;
        while (suffocateTimer_ >= 0.5f) {
            suffocateTimer_ -= 0.5f;
            player_->damage(1);
            spdlog::info("窒息！HP={}/{}", player_->health(), player_->maxHealth());
        }
    } else {
        suffocateTimer_ = 0.0f;
        if (player_->health() < player_->maxHealth()) {
            regenTimer_ += dt;
            while (regenTimer_ >= 4.0f) {
                regenTimer_ -= 4.0f;
                player_->heal(1);
            }
        } else {
            regenTimer_ = 0.0f;
        }
    }

    if (player_->isDead()) {
        respawnPlayer();
    }
}

void App::respawnPlayer() {
    if (!player_) return;
    glm::vec3 target = spawnPointSet_ ? spawnPoint_
                                       : glm::vec3(Chunk::kSizeX * 0.5f, 110.0f, Chunk::kSizeZ * 0.5f);
    int sy = static_cast<int>(std::floor(target.y));
    while (sy < Chunk::kSizeY - 1) {
        const Block here = chunkManager_->worldGet(static_cast<int>(std::floor(target.x)), sy,
                                                    static_cast<int>(std::floor(target.z)));
        const Block above = chunkManager_->worldGet(static_cast<int>(std::floor(target.x)), sy + 1,
                                                     static_cast<int>(std::floor(target.z)));
        if (here == Block::Air && above == Block::Air) break;
        ++sy;
    }
    target.y = static_cast<float>(sy);
    player_->setFootPos(target);
    player_->resetHealth();
    suffocateTimer_ = 0.0f;
    regenTimer_ = 0.0f;
    spdlog::info("玩家死亡，重生 ({:.1f},{:.1f},{:.1f})", target.x, target.y, target.z);
}

void App::renderHealthBar() {
    if (!player_) return;
    auto& hud = renderer_->hud();
    const int hp = player_->health();
    constexpr int kHearts = 10;
    const float heartSize = 0.038f;
    const float gap = 0.006f;
    const float total = kHearts * heartSize + (kHearts - 1) * gap;
    const float x0 = -total * 0.5f;
    const float y = 0.745f;
    const glm::vec3 dark(0.18f, 0.04f, 0.04f);
    const glm::vec3 red(0.92f, 0.18f, 0.20f);
    const glm::vec3 half(0.78f, 0.18f, 0.20f);

    for (int i = 0; i < kHearts; ++i) {
        const float lx = x0 + i * (heartSize + gap);
        const float rx = lx + heartSize;
        const float ty = y;
        const float by = y + heartSize;
        hud.addRect(lx, ty, rx, by, dark);

        const int hpForHeart = hp - i * 2;
        if (hpForHeart >= 2) {
            const float pad = heartSize * 0.10f;
            hud.addRect(lx + pad, ty + pad, rx - pad, by - pad, red);
        } else if (hpForHeart == 1) {
            const float pad = heartSize * 0.10f;
            hud.addRect(lx + pad, ty + pad, lx + heartSize * 0.5f, by - pad, half);
        }
    }
}

void App::tryPlaceBlock() {
    auto reader = [this](int x, int y, int z) { return chunkManager_->worldGet(x, y, z); };
    const auto hit = Raycaster::cast(reader, camera_->position(),
                                      camera_->forward(), kReachDistance);
    if (!hit) return;
    const glm::ivec3 pos = hit->blockPos + hit->normal;

    if (player_) {
        constexpr float kHalfWidth = 0.30f;
        constexpr float kHeight = 1.80f;
        const glm::vec3 foot = player_->footPos();
        const glm::vec3 pmin{foot.x - kHalfWidth, foot.y, foot.z - kHalfWidth};
        const glm::vec3 pmax{foot.x + kHalfWidth, foot.y + kHeight, foot.z + kHalfWidth};
        const glm::vec3 bmin{static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z)};
        const glm::vec3 bmax{bmin.x + 1.0f, bmin.y + 1.0f, bmin.z + 1.0f};
        const bool overlap = (pmin.x < bmax.x && pmax.x > bmin.x) &&
                             (pmin.y < bmax.y && pmax.y > bmin.y) &&
                             (pmin.z < bmax.z && pmax.z > bmin.z);
        if (overlap) {
            spdlog::info("放置被拒：方块会覆盖玩家身体");
            return;
        }
    }

    const Block b = kHotbarBlocks[selectedSlot_];
    chunkManager_->worldSet(pos.x, pos.y, pos.z, b);
    spdlog::info("放置 {} ({},{},{})", blockName(b), pos.x, pos.y, pos.z);
    if (hasGravity(b)) {
        applySandGravity(pos.x, pos.y - 1, pos.z);
    }
    rebuildAroundBlock(pos.x, pos.z);
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
            } else if (action == Menu::Action::OpenOthers) {
                enterOthers();
            } else if (action == Menu::Action::Quit) {
                glfwSetWindowShouldClose(window_->handle(), GLFW_TRUE);
            }
        } else if (state_ == State::Others) {
            const glm::dvec2 px = input_->mousePosPixels();
            const float ndcX = static_cast<float>(px.x / window_->width()) * 2.0f - 1.0f;
            const float ndcY = static_cast<float>(px.y / window_->height()) * 2.0f - 1.0f;
            othersHover_ = (othersStep_ != 1) ? othersHitTest(ndcX, ndcY) : -1;

            if (input_->isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                if (othersStep_ == 2) { enterMenu(); }
                else if (othersStep_ == 1) othersStep_ = 0;
                else enterMenu();
            }

            input_->setTextInputEnabled(othersStep_ == 1);

            if (othersStep_ == 2 && rpc_) {
                rpc_->pollAsync(1000, 600);
            }

            if (input_->isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                const int hit = othersHitTest(ndcX, ndcY);
                if (othersStep_ == 0) {
                    if (hit == 0) {
                        othersStep_ = 1;
                        othersActiveField_ = 0;
                    } else if (hit == 1) {
                        enterMenu();
                    }
                } else if (othersStep_ == 2) {
                    if (hit == 0) {
                        fetchAndApplyChunkFromRpc(0, 0);
                    } else if (hit == 1) {
                        enterMenu();
                    }
                } else {
                    if (hit >= 100 && hit <= 102) {
                        othersActiveField_ = hit - 100;
                    } else if (hit == 0) {
                        launchEmptyDea();
                    } else if (hit == 1) {
                        enterMenu();
                    } else {
                        othersActiveField_ = -1;
                    }
                }
            }

            if (othersStep_ == 1 && othersActiveField_ >= 0 && othersActiveField_ < 3) {
                auto& f = othersFields_[othersActiveField_];
                const size_t kMaxLen = (othersActiveField_ == 2) ? 8192 : 128;

                const auto& typed = input_->typedChars();
                if (!typed.empty()) {
                    f += typed;
                    if (f.size() > kMaxLen) f.resize(kMaxLen);
                }

                const bool ctrl = input_->isKeyDown(GLFW_KEY_LEFT_CONTROL) ||
                                   input_->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
                if (ctrl && input_->isKeyJustPressed(GLFW_KEY_V)) {
                    const char* clip = glfwGetClipboardString(window_->handle());
                    if (clip != nullptr) {
                        f += clip;
                        if (f.size() > kMaxLen) f.resize(kMaxLen);
                    }
                }

                if (input_->isKeyJustPressed(GLFW_KEY_BACKSPACE) ||
                    input_->isKeyDown(GLFW_KEY_BACKSPACE)) {
                    static int holdTick = 0;
                    if (input_->isKeyJustPressed(GLFW_KEY_BACKSPACE) || (++holdTick % 3) == 0) {
                        if (!f.empty()) f.pop_back();
                    }
                }
                if (input_->isKeyJustPressed(GLFW_KEY_TAB)) {
                    othersActiveField_ = (othersActiveField_ + 1) % 3;
                }
            }
        } else if (state_ == State::Paused) {
            const glm::dvec2 px = input_->mousePosPixels();
            const float ndcX = static_cast<float>(px.x / window_->width()) * 2.0f - 1.0f;
            const float ndcY = static_cast<float>(px.y / window_->height()) * 2.0f - 1.0f;
            pauseHover_ = pauseHitTest(ndcX, ndcY);

            if (input_->isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                enterInGame();
            } else if (input_->isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) && pauseHover_ >= 0) {
                if (pauseHover_ == 0) {
                    enterInGame();
                } else if (pauseHover_ == 1) {
                    enterMenu();
                } else if (pauseHover_ == 2) {
                    glfwSetWindowShouldClose(window_->handle(), GLFW_TRUE);
                }
            }
        } else {
            if (input_->isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                enterPaused();
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

        if (state_ == State::InGame || state_ == State::Others) {
            chunkManager_->update(camera_->position(), 4);
        }

        particles_->update(dt);
        particles_->uploadGeometry();

        if (falling_) {
            auto fallReader = [this](int x, int y, int z) {
                return chunkManager_->worldGet(x, y, z);
            };
            auto onLand = [this](const glm::ivec3& pos, Block block) {
                if (chunkManager_->worldGet(pos.x, pos.y, pos.z) == Block::Air) {
                    chunkManager_->worldSet(pos.x, pos.y, pos.z, block);
                    rebuildAroundBlock(pos.x, pos.z);
                }
            };
            falling_->update(dt, fallReader, onLand);
            tryPushPlayerFromFallingBlocks();
            falling_->uploadGeometry();
        }

        if (mirrorRunning_.load() && player_) {
            const glm::vec3 fp = player_->footPos();
            const int playerCx = floorDivLocal(static_cast<int>(std::floor(fp.x)), Chunk::kSizeX);
            const int playerCz = floorDivLocal(static_cast<int>(std::floor(fp.z)), Chunk::kSizeZ);
            mirrorCenterCx_.store(playerCx);
            mirrorCenterCz_.store(playerCz);

            if (udp_ && (playerCx != lastBotMoveCx_ || playerCz != lastBotMoveCz_)) {
                udp_->sendBotMove(static_cast<int32_t>(std::floor(fp.x)),
                                   static_cast<int32_t>(std::floor(fp.z)));
                lastBotMoveCx_ = playerCx;
                lastBotMoveCz_ = playerCz;
            }
        }
        drainMirrorQueue();

        if (state_ == State::InGame && player_) {
            updatePlayerHealth(dt);
        }

        if (state_ == State::InGame) {
            auto reader = [this](int x, int y, int z) {
                return chunkManager_->worldGet(x, y, z);
            };
            const auto hit = Raycaster::cast(reader, camera_->position(),
                                              camera_->forward(), kReachDistance);
            if (hit) {
                renderer_->setSelectedBlock(true, hit->blockPos);
            } else {
                renderer_->setSelectedBlock(false, {});
            }

            const glm::vec3 eye = camera_->position();
            const Block atEye = chunkManager_->worldGet(
                static_cast<int>(std::floor(eye.x)),
                static_cast<int>(std::floor(eye.y)),
                static_cast<int>(std::floor(eye.z)));
            renderer_->setUnderwater(atEye == Block::Water);
        } else {
            renderer_->setSelectedBlock(false, {});
            renderer_->setUnderwater(false);
        }

        renderer_->setHotbar(selectedSlot_, colors);
        const bool wallpaperOn = (state_ == State::Menu) || (state_ == State::Others);
        const bool drawMenuButtons = (state_ == State::Menu);
        renderer_->setMenuVisible(wallpaperOn, menu_->hoverIndex(), drawMenuButtons);

        const bool showHud = true;
        renderer_->setHudVisible(showHud);

        if (showHud) {
            auto& hud = renderer_->hud();
            hud.clear();

            if (state_ == State::Menu) {
                const glm::vec3 white(1.0f);
                const glm::vec3 dim(0.65f, 0.70f, 0.80f);
                const glm::vec3 splashYellow(1.00f, 0.93f, 0.20f);
                const glm::vec3 shadow(0.04f, 0.06f, 0.10f);

                const float titlePix = 0.022f;
                const std::string title = "NENET";
                const float titleW = static_cast<float>(title.size()) * kFontAdvance * titlePix;
                const float titleH = kFontHeight * titlePix;
                const float titleY = -0.62f - titleH * 0.5f;
                hud.addText(title, -titleW * 0.5f + 0.012f, titleY + 0.012f, titlePix, shadow);
                hud.addText(title, -titleW * 0.5f, titleY, titlePix, white);

                const float splashPix = 0.0060f;
                const std::string splash = "100% VULKAN!";
                const float splashW = static_cast<float>(splash.size()) * kFontAdvance * splashPix;
                hud.addText(splash, titleW * 0.5f - splashW * 0.6f - 0.02f,
                             titleY + 0.04f, splashPix, splashYellow);

                const float subPix = 0.0045f;
                const std::string sub = "A VOXEL SANDBOX";
                const float subW = static_cast<float>(sub.size()) * kFontAdvance * subPix;
                hud.addText(sub, -subW * 0.5f, -0.36f, subPix, dim);

                const float btnTextPix = 0.0070f;
                const float btnTextH = kFontHeight * btnTextPix;
                const char* btnLabels[3] = {"PLAY", "OTHERS", "QUIT"};
                const float btnCenterY[3] = {-0.08f, 0.06f, 0.20f};
                for (int i = 0; i < 3; ++i) {
                    const std::string lbl = btnLabels[i];
                    const float lblW = static_cast<float>(lbl.size()) * kFontAdvance * btnTextPix;
                    const glm::vec3 col = (menu_->hoverIndex() == i) ? white : glm::vec3(0.96f);
                    hud.addText(lbl, -lblW * 0.5f, btnCenterY[i] - btnTextH * 0.5f, btnTextPix, col);
                }

                const float tipPix = 0.0035f;
                const std::string tip1 = "MOVE WASD   LOOK MOUSE   JUMP SPACE";
                const std::string tip2 = "BREAK LMB   PLACE RMB   1-6 SLOT   F3 DEBUG";
                const float tip1W = static_cast<float>(tip1.size()) * kFontAdvance * tipPix;
                const float tip2W = static_cast<float>(tip2.size()) * kFontAdvance * tipPix;
                hud.addText(tip1, -tip1W * 0.5f, 0.36f, tipPix, dim);
                hud.addText(tip2, -tip2W * 0.5f, 0.42f, tipPix, dim);

                const float verPix = 0.0030f;
                const std::string verL = "NENET 0.1";
                const std::string verR = "COPYRIGHT NENET";
                hud.addText(verL, -0.98f, 0.92f, verPix, glm::vec3(0.85f));
                const float verRW = static_cast<float>(verR.size()) * kFontAdvance * verPix;
                hud.addText(verR, 0.98f - verRW, 0.92f, verPix, glm::vec3(0.85f));
            } else if (state_ == State::Paused) {
                renderPauseMenu();
            } else if (state_ == State::Others) {
                renderOthersScreen();
            } else {
                const glm::vec3 white(1.0f);
                const float thick = 0.0025f;
                const float length = 0.018f;
                const float aspect = static_cast<float>(window_->width())
                                   / static_cast<float>(window_->height());
                hud.addRect(-length, -thick, length, thick, white);
                hud.addRect(-thick / aspect, -length * aspect,
                             thick / aspect,  length * aspect, white);

                if (hudVisible_) {
                    const float pix = 0.0035f;
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

                if (state_ == State::InGame) {
                    renderHealthBar();
                }
            }

            hud.upload();
        }

        renderer_->drawFrame();
    }

    vk_->waitIdle();
    return 0;
}

}
