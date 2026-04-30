#include "fly_controller.h"

#include "../io/input.h"
#include "../render/camera.h"

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace nenet {

void FlyController::update(Camera& cam, Input& input, float dt) {
    const glm::vec2 m = input.mouseDelta();
    if (m.x != 0.0f || m.y != 0.0f) {
        cam.rotate(m.x * sensitivity_, -m.y * sensitivity_);
    }

    glm::vec3 dir(0.0f);
    const glm::vec3 fwd = cam.forward();
    const glm::vec3 fwdFlat = glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z));
    const glm::vec3 right = cam.right();

    if (input.isKeyDown(GLFW_KEY_W)) dir += fwdFlat;
    if (input.isKeyDown(GLFW_KEY_S)) dir -= fwdFlat;
    if (input.isKeyDown(GLFW_KEY_A)) dir -= right;
    if (input.isKeyDown(GLFW_KEY_D)) dir += right;
    if (input.isKeyDown(GLFW_KEY_SPACE)) dir.y += 1.0f;
    if (input.isKeyDown(GLFW_KEY_LEFT_CONTROL)) dir.y -= 1.0f;

    if (glm::length(dir) > 0.0001f) {
        dir = glm::normalize(dir);
        float v = speed_;
        if (input.isKeyDown(GLFW_KEY_LEFT_SHIFT)) v *= boost_;
        cam.translate(dir * (v * dt));
    }
}

}
