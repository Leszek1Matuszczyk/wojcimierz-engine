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

    // Granice wymiarów kształtu po przemnożeniu przez skalę (#22). Jolt liczy na floatach
    // i przy ogromnych kształtach daje NaN. Wojcimierz zna miarę każdej rzeczy.
    static constexpr float kMaxShapeExtent = 1e4f; // 10 km
    static constexpr float kMinShapeExtent = 1e-4f;

protected:
    // Przycina (wartość * skala) liczone w double do [kMin, kMax]. Ostrzega raz na collider.
    float LimitExtent(double scaled, const char* what) const;
    float LimitOffset(double scaled, const char* what) const;
    glm::vec3 CenterOf(const glm::vec3& sanitizedScale) const;
    // Wywołaj na początku i na końcu BuildShape. Gdy wymiary wrócą do zakresu, kasujemy flagę
    // ostrzeżenia, więc kolejne wyjście poza zakres znów ostrzeże.
    void BeginLimits() const { clippedThisBuild_ = false; }
    void EndLimits() const { if (!clippedThisBuild_) warnedExtent_ = false; }

private:
    mutable bool warnedExtent_ = false;
    mutable bool clippedThisBuild_ = false;
    void WarnExtent(const char* what, double v) const;

public:
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
