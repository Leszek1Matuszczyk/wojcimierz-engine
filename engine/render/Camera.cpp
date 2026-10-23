#include "render/Camera.h"
#include "scene/Transform.h"

#include <glm/gtc/matrix_transform.hpp>

namespace wz {

glm::mat4 Camera::GetViewMatrix() const {
    // Widok = odwrotnosc pozy kamery bez skali.
    const Transform& t = transform();
    const glm::mat4 rot = glm::mat4_cast(t.GetRotation());
    const glm::mat4 trans = glm::translate(glm::mat4(1.0f), t.GetPosition());
    return glm::inverse(trans * rot);
}

glm::mat4 Camera::GetProjectionMatrix(float aspect) const {
    if (orthographic) {
        const float h = orthographicSize, w = h * aspect;
        return glm::ortho(-w, w, -h, h, nearClipPlane, farClipPlane);
    }
    return glm::perspective(glm::radians(fieldOfView), aspect, nearClipPlane, farClipPlane);
}

} // namespace wz
