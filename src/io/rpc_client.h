#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace nenet {

class RpcClient {
public:
    explicit RpcClient(std::string baseUrl);

    void setBaseUrl(std::string baseUrl);
    [[nodiscard]] std::string baseUrl() const;

    void pollAsync(int intervalMs = 1000, int timeoutMs = 600);

    [[nodiscard]] std::string statusBody() const;
    [[nodiscard]] std::string versionBody() const;
    [[nodiscard]] bool isReachable() const noexcept { return reachable_; }

    [[nodiscard]] std::vector<uint8_t> fetchBinary(const std::string& path, int timeoutMs = 5000) const;

private:
    void kickIfDue(int intervalMs, int timeoutMs);
    static std::string fetchOnce(const std::string& url, int timeoutMs);

    mutable std::mutex mu_;
    std::string baseUrl_;
    std::string statusBody_;
    std::string versionBody_;

    std::future<std::pair<std::string, std::string>> fut_;
    std::chrono::steady_clock::time_point nextFetch_{};
    bool reachable_{false};
};

}
