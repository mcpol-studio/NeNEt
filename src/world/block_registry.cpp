#include "block_registry.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <string>

namespace nenet {

BlockRegistry& BlockRegistry::instance() {
    static BlockRegistry r;
    return r;
}

bool BlockRegistry::load(const std::filesystem::path& txtPath) {
    std::ifstream ifs(txtPath);
    if (!ifs) {
        spdlog::warn("BlockRegistry 读取失败: {}", txtPath.string());
        return false;
    }
    nameToSlot_.clear();
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') {

            if (line.rfind("# grid", 0) == 0) {
                std::istringstream iss(line);
                std::string h, gk, tk;
                int g = 0, t = 0;
                iss >> h >> gk >> g >> tk >> t;
                if (g > 0) grid_ = g;
            }
            continue;
        }

        std::istringstream iss(line);
        int slot = -1;
        std::string name;
        int hasAlpha = 0;
        iss >> slot >> name >> hasAlpha;
        if (slot >= 0 && !name.empty()) {
            nameToSlot_[name] = slot;
        }
    }
    spdlog::info("BlockRegistry 已加载 {} 个 tile, grid={}", nameToSlot_.size(), grid_);
    return !nameToSlot_.empty();
}

int BlockRegistry::slotByName(const std::string& name) const {
    auto it = nameToSlot_.find(name);
    return (it == nameToSlot_.end()) ? -1 : it->second;
}

int BlockRegistry::slotOr(const std::string& name, const std::string& fallback) const {
    int s = slotByName(name);
    if (s >= 0) return s;
    s = slotByName(fallback);
    if (s >= 0) return s;
    return 0;
}

}
