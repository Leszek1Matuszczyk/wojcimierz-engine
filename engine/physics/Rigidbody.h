// ============================================================
//  Wojcimierz Engine: physics/Rigidbody.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "physics/IPhysicsBackend.h"
#include "physics/PhysicsTypes.h"
#include "scene/Component.h"

namespace wz {

class PhysicsWorld;

// Odpowiednik UnityEngine.Rigidbody. Wymaga co najmniej jednego Collidera na
// tym samym GameObjecie - inaczej cialo nie powstaje (jak w Unity bez ksztaltu
// cialo nie koliduje; u nas po prostu nie jest symulowane).
//
// Wlasciwosci konfiguracyjne (mass, bodyType, ...) sa publicznymi polami
// pod REFLECT; ich zmiana powoduje przebudowe ciala w nastepnym kroku.
// Predkosci i sily ida przez metody, bo dzialaja na zywym ciele.
class Rigidbody final : public Component {
public:
    float mass = 1.0f;
    BodyType bodyType = BodyType::Dynamic;
    bool useGravity = true;
    float linearDamping = 0.0f;  // Unity: drag
    float angularDamping = 0.05f; // Unity: angularDrag
    bool freezeRotation = false;

    glm::vec3 GetVelocity() const;
    void SetVelocity(const glm::vec3& v);
    glm::vec3 GetAngularVelocity() const;
    void SetAngularVelocity(const glm::vec3& w);

    void AddForce(const glm::vec3& f, ForceMode mode = ForceMode::Force);
    void AddTorque(const glm::vec3& t, ForceMode mode = ForceMode::Force);

    // Dla cial kinematycznych: plynny ruch do pozy w nastepnym kroku fizyki.
    // Dla dynamicznych dziala jak teleport.
    void MovePosition(const glm::vec3& worldPosition);
    void MoveRotation(const glm::quat& worldRotation);

    bool IsSleeping() const;
    void WakeUp();
    bool HasBody() const { return world_ != nullptr && body_ != kInvalidBody; }

    void OnDestroy() override;

private:
    friend class PhysicsWorld;
    // Odrzuca NaN/inf z gameplayu (issue #16). Ostrzezenie raz na cialo.
    bool Reject(const char* what, const glm::vec3& v);
    bool Reject(const char* what, const glm::quat& q);
    bool warnedNonFinite_ = false;
    // Wartosci zadane przed powstaniem ciala - aplikowane przy jego tworzeniu.
    glm::vec3 pendingVelocity_{0.0f};
    glm::vec3 pendingAngularVelocity_{0.0f};
    glm::vec3 pendingForce_{0.0f};
    glm::vec3 pendingImpulse_{0.0f};
    PhysicsWorld* world_ = nullptr;
    BodyHandle body_ = kInvalidBody;
};

} // namespace wz
