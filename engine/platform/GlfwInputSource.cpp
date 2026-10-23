#include "platform/GlfwInputSource.h"

#include <GLFW/glfw3.h>

namespace wz {

bool GlfwInputSource::IsKeyPressed(KeyCode key) const {
    const int k = static_cast<int>(key);
    if (k < GLFW_KEY_SPACE || k > GLFW_KEY_LAST) return false;
    return glfwGetKey(window_, k) == GLFW_PRESS;
}

bool GlfwInputSource::IsMouseButtonPressed(int button) const {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return glfwGetMouseButton(window_, button) == GLFW_PRESS;
}

glm::vec2 GlfwInputSource::GetMousePosition() const {
    double x = 0, y = 0;
    glfwGetCursorPos(window_, &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

} // namespace wz
