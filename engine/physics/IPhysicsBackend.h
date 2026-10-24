// ============================================================
//  Wojcimierz Engine: physics/IPhysicsBackend.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "physics/PhysicsTypes.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace wz {

// Opis jednego ksztaltu ciala w ukladzie lokalnym ciala (skala juz wliczona).
struct ShapeDesc {
    enum class Kind : std::uint8_t { Box, Sphere, Capsule } kind = Kind::Box;
    glm::vec3 halfExtents{0.5f}; // Box
    float radius = 0.5f;         // Sphere, Capsule
    float halfHeight = 0.5f;     // Capsule: polowa dlugosci czesci walcowej (os Y)
    glm::vec3 center{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct BodyDesc {
    BodyType type = BodyType::Static;
    std::vector<ShapeDesc> shapes;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 linearVelocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float mass = 1.0f;
    float linearDamping = 0.0f;
    float angularDamping = 0.05f;
    float friction = 0.6f;
    float restitution = 0.0f;
    float gravityFactor = 1.0f;
    bool isSensor = false;  // trigger: zdarzenia bez odpowiedzi kolizyjnej
    bool freezeRotation = false;
    bool continuousCollision = false; // CCD (sweep) dla szybkich cial dynamicznych
    std::uint64_t userData = 0; // identyfikator po stronie silnika (wz::PhysicsWorld)
};

using BodyHandle = std::uint32_t;
inline constexpr BodyHandle kInvalidBody = 0xffffffffu;

// Zdarzenie kontaktu miedzy dwoma cialami (na poziomie cial, nie podksztaltow).
struct ContactEvent {
    enum class Type : std::uint8_t { Added, Removed } type = Type::Added;
    BodyHandle a = kInvalidBody;
    BodyHandle b = kInvalidBody;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f}; // od a do b
    glm::vec3 relativeVelocity{0.0f}; // v_a - v_b w chwili zetkniecia (przed solverem)
    bool sensor = false;
    // Removed tylko dlatego, ze oba ciala zasnely (Jolt usuwa wtedy kontakty).
    // Unity w takiej sytuacji NIE wysyla OnCollisionExit - PhysicsWorld tez nie.
    bool sleeping = false;
};

struct BackendRaycastHit {
    BodyHandle body = kInvalidBody;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    float distance = 0.0f;
};

// Abstrakcyjny backend fizyki. Gameplay i edytor nigdy go nie widza -
// rozmawiaja z komponentami i PhysicsWorld. Dzieki temu Jolt da sie podmienic.
class IPhysicsBackend {
public:
    virtual ~IPhysicsBackend() = default;

    virtual BodyHandle CreateBody(const BodyDesc& desc) = 0;
    virtual void DestroyBody(BodyHandle body) = 0;

    // Teleport (dla dynamicznych i statycznych). Budzi ciało. Prędkość liniowa i kątowa
    // ZOSTAJĄ, a kontakty nie są kasowane. Solver przeliczy je w następnym kroku.
    virtual void SetPose(BodyHandle body, const glm::vec3& p, const glm::quat& r) = 0;
    // To samo, ale nie budzi śpiącego ciała. Dla drobnych korekt pozy z gameplayu (#29):
    // inaczej LookAt co klatkę na leżącej kuli nie pozwoliłby jej zasnąć.
    // Domyślnie wywołuje SetPose, żeby atrapy w testach nie musiały tego implementować.
    virtual void SetPoseNoWake(BodyHandle body, const glm::vec3& p, const glm::quat& r) { SetPose(body, p, r); }
    // Ruch kinematyczny: cialo dojdzie do pozy w ciagu nastepnego kroku dt.
    virtual void MoveKinematic(BodyHandle body, const glm::vec3& p, const glm::quat& r, float dt) = 0;
    virtual void GetPose(BodyHandle body, glm::vec3& p, glm::quat& r) const = 0;

    virtual glm::vec3 GetLinearVelocity(BodyHandle body) const = 0;
    virtual void SetLinearVelocity(BodyHandle body, const glm::vec3& v) = 0;
    virtual glm::vec3 GetAngularVelocity(BodyHandle body) const = 0;
    virtual void SetAngularVelocity(BodyHandle body, const glm::vec3& w) = 0;
    virtual void AddForce(BodyHandle body, const glm::vec3& f) = 0;
    virtual void AddImpulse(BodyHandle body, const glm::vec3& j) = 0;
    virtual void AddTorque(BodyHandle body, const glm::vec3& t) = 0;
    virtual void AddAngularImpulse(BodyHandle body, const glm::vec3& j) = 0;
    virtual float GetMass(BodyHandle body) const = 0;
    virtual bool IsActive(BodyHandle body) const = 0;
    virtual void Activate(BodyHandle body) = 0;

    virtual void SetGravity(const glm::vec3& g) = 0;
    virtual glm::vec3 GetGravity() const = 0;

    virtual void Step(float dt) = 0;
    // Zdarzenia kontaktu zebrane od ostatniego wywolania (zwraca i czysci).
    virtual std::vector<ContactEvent> DrainContactEvents() = 0;

    virtual std::optional<BackendRaycastHit> Raycast(const glm::vec3& origin, const glm::vec3& dir,
                                                     float maxDistance, bool hitTriggers) const = 0;
    virtual std::size_t GetBodyCount() const = 0;
};

std::unique_ptr<IPhysicsBackend> CreateJoltBackend();

} // namespace wz
