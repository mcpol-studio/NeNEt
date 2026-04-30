#pragma once

#include <spdlog/spdlog.h>

namespace nenet::log {

inline void init() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);
}

}
