// ============================================================
//  Wojcimierz Engine: physics/Collider.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "scene/Component.h"

#include <glm/glm.hpp>

namespace wz {

struct ShapeDesc;

// Baza colliderow (odpowiednik UnityEngine.Collider).
// Collider bez Rigidbody na tym samym GameObjecie tworzy cialo statyczne.
// Pola sa publiczne, zeby inspektor i serializator mogly je opisac przez REFLECT;
// PhysicsWorld wykrywa ich zmiane i sam przebudowuje cialo.
class Collider : public Component {
public:
    bool isTrigger = false;
    glm::vec3 center{0.0f}; // przesuniecie wzgledem Transformu (w ukladzie lokalnym)
    float friction = 0.6f;
    float restitution = 0.0f; // "bounciness"

    void OnDestroy() override;

    // Opis ksztaltu w ukladzie lokalnym ciala, z uwzglednieniem skali Transformu.
    virtual ShapeDesc BuildShape(const glm::vec3& lossyScale) const = 0;
};

class BoxCollider final : public Collider {
public:
    glm::vec3 size{1.0f};
    ShapeDesc BuildShape(const glm::vec3& lossyScale) const override;
};

class SphereCollider final : public Collider {
public:
    float radius = 0.5f;
    ShapeDesc BuildShape(const glm::vec3& lossyScale) const override;
};

// Kapsula wzdluz lokalnej osi Y (jak w Unity z direction = Y-Axis).
class CapsuleCollider final : public Collider {
public:
    float radius = 0.5f;
    float height = 2.0f; // calkowita wysokosc razem z polkulami
    ShapeDesc BuildShape(const glm::vec3& lossyScale) const override;
};

} // namespace wz
