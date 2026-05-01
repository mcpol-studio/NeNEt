#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace nenet {

class Input;

class Menu {
public:
    enum class Action { None, StartGame, OpenOthers, Quit };

    Menu() = default;

    Action update(Input& input, int windowWidth, int windowHeight);

    [[nodiscard]] int hoverIndex() const noexcept { return hoverIndex_; }

    static constexpr int kButtonCount = 3;

private:
    int hoverIndex_{-1};
};

}
