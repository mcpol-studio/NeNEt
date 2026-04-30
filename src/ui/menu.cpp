#include "menu.h"

#include "../io/input.h"

#include <GLFW/glfw3.h>

namespace nenet {

namespace {

struct ButtonRect {
    float x, y, w, h;
};

constexpr ButtonRect kRects[Menu::kButtonCount] = {
    {-0.22f, -0.06f,  0.44f, 0.12f},
    {-0.22f,  0.12f,  0.44f, 0.12f},
};

bool inside(const ButtonRect& r, float ndcX, float ndcY) {
    return ndcX >= r.x && ndcX <= r.x + r.w &&
           ndcY >= r.y && ndcY <= r.y + r.h;
}

}

Menu::Action Menu::update(Input& input, int windowWidth, int windowHeight) {
    const glm::dvec2 px = input.mousePosPixels();
    const float ndcX = static_cast<float>(px.x / windowWidth) * 2.0f - 1.0f;
    const float ndcY = static_cast<float>(px.y / windowHeight) * 2.0f - 1.0f;

    hoverIndex_ = -1;
    for (int i = 0; i < kButtonCount; ++i) {
        if (inside(kRects[i], ndcX, ndcY)) {
            hoverIndex_ = i;
            break;
        }
    }

    if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) && hoverIndex_ >= 0) {
        if (hoverIndex_ == 0) return Action::StartGame;
        if (hoverIndex_ == 1) return Action::Quit;
    }
    return Action::None;
}

}
