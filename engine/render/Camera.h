// ============================================================
//  Wojcimierz Engine: render/Camera.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "scene/Component.h"

#include <glm/glm.hpp>

namespace wz {

// Odpowiednik UnityEngine.Camera. Kamera patrzy wzdluz -Z swojego Transformu.
class Camera : public Component {
public:
    float fieldOfView = 60.0f; // pionowe FOV w stopniach
    float nearClipPlane = 0.1f;
    float farClipPlane = 100.0f;
    bool orthographic = false;
    float orthographicSize = 5.0f; // polowa wysokosci widoku
    glm::vec4 backgroundColor{0.10f, 0.11f, 0.14f, 1.0f};
    int depth = 0; // kamera o najmniejszej glebokosci renderuje pierwsza

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspect) const;
    // Ta sama projekcja z innymi plaszczyznami odciecia (np. wycinek do cieni).
    glm::mat4 GetProjectionMatrix(float aspect, float nearPlane, float farPlane) const;
};

} // namespace wz
