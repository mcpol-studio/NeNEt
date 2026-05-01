#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace nenet {

class UdpClient {
public:
    UdpClient();
    ~UdpClient();

    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;

    bool start(const std::string& serverHostPort);
    void stop();

    struct ChunkData {
        int32_t cx;
        int32_t cz;
        std::vector<uint8_t> data;
    };

    std::vector<ChunkData> drainCompleted();

    void sendBotMove(int32_t worldX, int32_t worldZ);

private:
    void recvLoop();
    void heartbeatLoop();
    void sendHello();

    struct ChunkBuffer {
        std::vector<uint8_t> data;
        std::array<uint8_t, 256> mask{};
        int received{0};
        std::chrono::steady_clock::time_point firstSeen;
    };

    std::atomic<bool> started_{false};
    std::atomic<bool> stop_{false};

#ifdef _WIN32

    std::atomic<uintptr_t> sock_{static_cast<uintptr_t>(~0ull)};
#else
    std::atomic<int> sock_{-1};
#endif

    std::thread recvThread_;
    std::thread heartbeatThread_;

    std::mutex reasmMu_;

    std::unordered_map<uint64_t, ChunkBuffer> reasm_;

    std::mutex completedMu_;
    std::vector<ChunkData> completed_;
};

}
