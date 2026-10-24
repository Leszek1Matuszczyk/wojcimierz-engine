// ============================================================
//  Wojcimierz Engine: render/Camera.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
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
    return GetProjectionMatrix(aspect, nearClipPlane, farClipPlane);
}

glm::mat4 Camera::GetProjectionMatrix(float aspect, float nearPlane, float farPlane) const {
    if (orthographic) {
        const float h = orthographicSize, w = h * aspect;
        return glm::ortho(-w, w, -h, h, nearPlane, farPlane);
    }
    return glm::perspective(glm::radians(fieldOfView), aspect, nearPlane, farPlane);
}

} // namespace wz
