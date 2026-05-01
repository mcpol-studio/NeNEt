#include "rpc_client.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdio>
#include <utility>

namespace nenet {

namespace {

std::string popenCapture(const std::string& cmd) {
    std::string out;
    FILE* fp = _popen(cmd.c_str(), "rb");
    if (fp == nullptr) return out;
    std::array<char, 4096> buf{};
    while (true) {
        const size_t n = std::fread(buf.data(), 1, buf.size(), fp);
        if (n == 0) break;
        out.append(buf.data(), n);
    }
    _pclose(fp);
    return out;
}

}

RpcClient::RpcClient(std::string baseUrl) : baseUrl_(std::move(baseUrl)) {}

void RpcClient::setBaseUrl(std::string baseUrl) {
    std::lock_guard<std::mutex> lk(mu_);
    baseUrl_ = std::move(baseUrl);
    reachable_ = false;
    statusBody_.clear();
    versionBody_.clear();
}

std::string RpcClient::baseUrl() const {
    std::lock_guard<std::mutex> lk(mu_);
    return baseUrl_;
}

std::string RpcClient::fetchOnce(const std::string& url, int timeoutMs) {
    char timeoutSec[16];
    std::snprintf(timeoutSec, sizeof(timeoutSec), "%.2f", timeoutMs / 1000.0);
    std::string cmd = "curl -s --max-time ";
    cmd += timeoutSec;
    cmd += " \"";
    cmd += url;
    cmd += "\" 2>nul";
    return popenCapture(cmd);
}

void RpcClient::kickIfDue(int intervalMs, int timeoutMs) {
    const auto now = std::chrono::steady_clock::now();
    if (fut_.valid()) {
        if (fut_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto pair = fut_.get();
            std::lock_guard<std::mutex> lk(mu_);
            statusBody_ = std::move(pair.first);
            versionBody_ = std::move(pair.second);
            reachable_ = !statusBody_.empty();
            nextFetch_ = now + std::chrono::milliseconds(intervalMs);
        }
        return;
    }
    if (now < nextFetch_) return;

    std::string base;
    {
        std::lock_guard<std::mutex> lk(mu_);
        base = baseUrl_;
    }
    if (base.empty()) return;

    fut_ = std::async(std::launch::async, [base, timeoutMs]() {
        std::string s = fetchOnce(base + "/status", timeoutMs);
        std::string v = fetchOnce(base + "/version", timeoutMs);
        return std::make_pair(std::move(s), std::move(v));
    });
}

void RpcClient::pollAsync(int intervalMs, int timeoutMs) {
    kickIfDue(intervalMs, timeoutMs);
}

std::string RpcClient::statusBody() const {
    std::lock_guard<std::mutex> lk(mu_);
    return statusBody_;
}

std::string RpcClient::versionBody() const {
    std::lock_guard<std::mutex> lk(mu_);
    return versionBody_;
}

std::vector<uint8_t> RpcClient::fetchBinary(const std::string& path, int timeoutMs) const {
    std::string base;
    {
        std::lock_guard<std::mutex> lk(mu_);
        base = baseUrl_;
    }
    if (base.empty()) return {};
    char timeoutSec[16];
    std::snprintf(timeoutSec, sizeof(timeoutSec), "%.2f", timeoutMs / 1000.0);
    std::string cmd = "curl -s --max-time ";
    cmd += timeoutSec;
    cmd += " \"";
    cmd += base;
    cmd += path;
    cmd += "\" 2>nul";
    FILE* fp = _popen(cmd.c_str(), "rb");
    if (fp == nullptr) return {};
    std::vector<uint8_t> out;
    out.reserve(65536);
    std::array<uint8_t, 4096> buf{};
    while (true) {
        const size_t n = std::fread(buf.data(), 1, buf.size(), fp);
        if (n == 0) break;
        out.insert(out.end(), buf.data(), buf.data() + n);
    }
    _pclose(fp);
    return out;
}

}
