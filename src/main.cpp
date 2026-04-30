#include "core/app.h"
#include <spdlog/spdlog.h>
#include <exception>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    try {
        nenet::App app;
        return app.run();
    } catch (const std::exception& e) {
        spdlog::critical("程序崩溃: {}", e.what());
        return 1;
    }
}
