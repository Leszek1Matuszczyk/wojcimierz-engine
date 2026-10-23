// =============================================================================
// Wojcimierz Engine - engine/platform/GlfwInputSource.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include "platform/Input.h"

struct GLFWwindow;

namespace wz {

// Zrodlo wejscia czytajace stan z okna GLFW (polling glfwGetKey/glfwGetMouseButton).
class GlfwInputSource final : public IInputSource {
public:
    explicit GlfwInputSource(GLFWwindow* window) : window_(window) {}
    bool IsKeyPressed(KeyCode key) const override;
    bool IsMouseButtonPressed(int button) const override;
    glm::vec2 GetMousePosition() const override;

private:
    GLFWwindow* window_;
};

} // namespace wz
