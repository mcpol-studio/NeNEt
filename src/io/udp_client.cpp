#include "udp_client.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

#include <cstring>
#include <chrono>

namespace nenet {

namespace {

#ifdef _WIN32
constexpr uintptr_t kInvalidSock = static_cast<uintptr_t>(~0ull);
struct WsaInit {
    bool ok{false};
    WsaInit() {
        WSADATA wsa{};
        ok = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    }
    ~WsaInit() {
        if (ok) WSACleanup();
    }
};

WsaInit& wsa() {
    static WsaInit g;
    return g;
}
#endif

constexpr uint16_t kMagic       = 0xCDEA;
constexpr uint8_t  kVersion     = 1;
constexpr uint8_t  kTypeHello   = 0;
constexpr uint8_t  kTypeChunk   = 1;
constexpr uint8_t  kTypeBotMove = 2;
constexpr uint16_t kTotalSlices = 256;
constexpr size_t   kHeaderBytes = 16;
constexpr size_t   kSliceBytes  = 256;
constexpr size_t   kChunkBytes  = static_cast<size_t>(kTotalSlices) * kSliceBytes;
constexpr size_t   kMaxPacket   = kHeaderBytes + kSliceBytes;
constexpr auto     kReasmTimeout = std::chrono::seconds(5);

inline uint16_t loadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline int32_t loadI32LE(const uint8_t* p) {
    uint32_t v = static_cast<uint32_t>(p[0])
               | (static_cast<uint32_t>(p[1]) << 8)
               | (static_cast<uint32_t>(p[2]) << 16)
               | (static_cast<uint32_t>(p[3]) << 24);
    return static_cast<int32_t>(v);
}
inline void storeU16LE(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}
inline void storeI32LE(uint8_t* p, int32_t v) {
    uint32_t u = static_cast<uint32_t>(v);
    p[0] = static_cast<uint8_t>(u & 0xFF);
    p[1] = static_cast<uint8_t>((u >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((u >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((u >> 24) & 0xFF);
}

}

UdpClient::UdpClient() {
#ifdef _WIN32

    (void)wsa();
    sock_.store(kInvalidSock);
#else
    sock_.store(-1);
#endif
}

UdpClient::~UdpClient() {
    stop();
}

bool UdpClient::start(const std::string& serverHostPort) {
    if (started_.load()) return true;

#ifdef _WIN32
    if (!wsa().ok) {
        spdlog::error("UdpClient: WSAStartup 失败");
        return false;
    }
#endif

    std::string host;
    std::string port;
    auto colon = serverHostPort.rfind(':');
    if (colon == std::string::npos) {
        spdlog::error("UdpClient: 非法 host:port '{}'", serverHostPort);
        return false;
    }
    host = serverHostPort.substr(0, colon);
    port = serverHostPort.substr(colon + 1);

    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
        spdlog::error("UdpClient: getaddrinfo 失败 host={} port={}", host, port);
        return false;
    }

#ifdef _WIN32
    SOCKET s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        spdlog::error("UdpClient: socket() 失败 err={}", WSAGetLastError());
        ::freeaddrinfo(res);
        return false;
    }
#else
    int s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) {
        spdlog::error("UdpClient: socket() 失败");
        ::freeaddrinfo(res);
        return false;
    }
#endif

    sockaddr_in localAddr{};
    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port        = htons(0);
    if (::bind(s, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr)) != 0) {
#ifdef _WIN32
        spdlog::error("UdpClient: bind 失败 err={}", WSAGetLastError());
        ::closesocket(s);
#else
        spdlog::error("UdpClient: bind 失败");
        ::close(s);
#endif
        ::freeaddrinfo(res);
        return false;
    }

    if (::connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
#ifdef _WIN32
        spdlog::error("UdpClient: connect 失败 err={}", WSAGetLastError());
        ::closesocket(s);
#else
        spdlog::error("UdpClient: connect 失败");
        ::close(s);
#endif
        ::freeaddrinfo(res);
        return false;
    }
    ::freeaddrinfo(res);

#ifdef _WIN32
    sock_.store(static_cast<uintptr_t>(s));
#else
    sock_.store(s);
#endif

    stop_.store(false);
    started_.store(true);

    recvThread_      = std::thread([this] { recvLoop(); });
    heartbeatThread_ = std::thread([this] { heartbeatLoop(); });

    spdlog::info("UdpClient: 已连接到 {}", serverHostPort);
    return true;
}

void UdpClient::stop() {
    if (!started_.load()) return;
    stop_.store(true);

#ifdef _WIN32
    uintptr_t s = sock_.exchange(kInvalidSock);
    if (s != kInvalidSock) {
        ::shutdown(static_cast<SOCKET>(s), SD_BOTH);
        ::closesocket(static_cast<SOCKET>(s));
    }
#else
    int s = sock_.exchange(-1);
    if (s >= 0) {
        ::shutdown(s, SHUT_RDWR);
        ::close(s);
    }
#endif

    if (recvThread_.joinable())      recvThread_.join();
    if (heartbeatThread_.joinable()) heartbeatThread_.join();

    {
        std::lock_guard<std::mutex> lk(reasmMu_);
        reasm_.clear();
    }
    {
        std::lock_guard<std::mutex> lk(completedMu_);
        completed_.clear();
    }
    started_.store(false);
}

void UdpClient::sendHello() {
#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(sock_.load());
    if (s == static_cast<SOCKET>(kInvalidSock)) return;
#else
    int s = sock_.load();
    if (s < 0) return;
#endif
    uint8_t buf[kHeaderBytes] = {};
    storeU16LE(buf + 0, kMagic);
    buf[2] = kVersion;
    buf[3] = kTypeHello;
    storeI32LE(buf + 4, 0);
    storeI32LE(buf + 8, 0);
    storeU16LE(buf + 12, 0);
    storeU16LE(buf + 14, 0);
    (void)::send(s, reinterpret_cast<const char*>(buf), static_cast<int>(sizeof(buf)), 0);
}

void UdpClient::sendBotMove(int32_t worldX, int32_t worldZ) {
#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(sock_.load());
    if (s == static_cast<SOCKET>(kInvalidSock)) return;
#else
    int s = sock_.load();
    if (s < 0) return;
#endif
    uint8_t buf[kHeaderBytes] = {};
    storeU16LE(buf + 0, kMagic);
    buf[2] = kVersion;
    buf[3] = kTypeBotMove;
    storeI32LE(buf + 4, worldX);
    storeI32LE(buf + 8, worldZ);
    storeU16LE(buf + 12, 0);
    storeU16LE(buf + 14, 0);
    (void)::send(s, reinterpret_cast<const char*>(buf), static_cast<int>(sizeof(buf)), 0);
}

void UdpClient::heartbeatLoop() {

    sendHello();
    while (!stop_.load()) {
        for (int i = 0; i < 50 && !stop_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (stop_.load()) break;
        sendHello();
    }
}

void UdpClient::recvLoop() {
    uint8_t buf[kMaxPacket + 64];
    while (!stop_.load()) {
#ifdef _WIN32
        SOCKET s = static_cast<SOCKET>(sock_.load());
        if (s == static_cast<SOCKET>(kInvalidSock)) break;
        int n = ::recv(s, reinterpret_cast<char*>(buf), static_cast<int>(sizeof(buf)), 0);
        if (n == SOCKET_ERROR) {
            if (stop_.load()) break;

            continue;
        }
#else
        int s = sock_.load();
        if (s < 0) break;
        ssize_t n = ::recv(s, buf, sizeof(buf), 0);
        if (n < 0) {
            if (stop_.load()) break;
            continue;
        }
#endif
        if (n < static_cast<int>(kHeaderBytes)) continue;

        const uint16_t magic   = loadU16LE(buf + 0);
        const uint8_t  version = buf[2];
        const uint8_t  type    = buf[3];
        const int32_t  cx      = loadI32LE(buf + 4);
        const int32_t  cz      = loadI32LE(buf + 8);
        const uint16_t seq     = loadU16LE(buf + 12);
        const uint16_t total   = loadU16LE(buf + 14);

        if (magic != kMagic) continue;
        if (version != kVersion) continue;
        if (type != kTypeChunk) continue;
        if (total != kTotalSlices) continue;
        if (seq >= kTotalSlices) continue;
        if (n != static_cast<int>(kHeaderBytes + kSliceBytes)) continue;

        const uint64_t key =
            (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
             static_cast<uint64_t>(static_cast<uint32_t>(cz));

        auto now = std::chrono::steady_clock::now();
        ChunkData done;
        bool isDone = false;
        {
            std::lock_guard<std::mutex> lk(reasmMu_);

            for (auto it = reasm_.begin(); it != reasm_.end(); ) {
                if (now - it->second.firstSeen > kReasmTimeout) {
                    it = reasm_.erase(it);
                } else {
                    ++it;
                }
            }

            auto it = reasm_.find(key);
            if (it == reasm_.end()) {
                ChunkBuffer cb;
                cb.data.assign(kChunkBytes, 0);
                cb.firstSeen = now;
                it = reasm_.emplace(key, std::move(cb)).first;
            }
            ChunkBuffer& cb = it->second;

            if (cb.mask[seq] == 0) {
                std::memcpy(cb.data.data() + static_cast<size_t>(seq) * kSliceBytes,
                            buf + kHeaderBytes, kSliceBytes);
                cb.mask[seq] = 1;
                cb.received++;
            }
            if (cb.received >= kTotalSlices) {
                done.cx = cx;
                done.cz = cz;
                done.data = std::move(cb.data);
                reasm_.erase(it);
                isDone = true;
            }
        }

        if (isDone) {
            std::lock_guard<std::mutex> lk(completedMu_);
            completed_.push_back(std::move(done));
        }
    }
}

std::vector<UdpClient::ChunkData> UdpClient::drainCompleted() {
    std::vector<ChunkData> out;
    std::lock_guard<std::mutex> lk(completedMu_);
    out.swap(completed_);
    return out;
}

}
