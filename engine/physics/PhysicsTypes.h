// ============================================================
//  Wojcimierz Engine: physics/PhysicsTypes.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace wz {

class GameObject;
class Rigidbody;
class Collider;

// Typ ciala - jak RigidbodyType2D / isKinematic w Unity.
enum class BodyType : std::uint8_t {
    Dynamic,   // symulowane: grawitacja, sily, kolizje
    Kinematic, // ruszane z gameplayu (MovePosition / Transform), popycha dynamiczne
    Static,    // nieruchome (np. podloga); GameObject z samym colliderem tez jest statyczny
};

// Odpowiednik UnityEngine.ForceMode.
enum class ForceMode : std::uint8_t {
    Force,          // sila ciagla [N], uwzglednia mase, dziala przez caly krok
    Acceleration,   // przyspieszenie [m/s^2], ignoruje mase
    Impulse,        // impuls [N*s], uwzglednia mase, natychmiastowy
    VelocityChange, // zmiana predkosci [m/s], ignoruje mase, natychmiastowa
};

// Wynik Physics.Raycast.
struct RaycastHit {
    GameObject* gameObject = nullptr;
    Rigidbody* rigidbody = nullptr; // nullptr dla statycznych colliderow
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    float distance = 0.0f;
};

// Dane przekazywane do OnCollision* (odpowiednik UnityEngine.Collision).
// Punkt i normalna pochodza z pierwszego punktu styku w chwili zetkniecia.
struct Collision {
    GameObject* gameObject = nullptr; // "ten drugi"
    Rigidbody* rigidbody = nullptr;   // rigidbody "tego drugiego" (moze byc nullptr)
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};           // od "tego drugiego" w strone odbiorcy
    glm::vec3 relativeVelocity{0.0f}; // predkosc odbiorcy minus predkosc drugiego
};

// Domieszka dla komponentow chcacych dostawac zdarzenia kolizji.
// Komponent dziedziczy po wz::Component ORAZ po CollisionListener, np.:
//   class Player : public Component, public CollisionListener { ... };
// Zdarzenia wysylane sa po kroku fizyki, w FixedUpdate (jak w Unity).
class CollisionListener {
public:
    virtual ~CollisionListener() = default;
    virtual void OnCollisionEnter(const Collision& /*c*/) {}
    virtual void OnCollisionStay(const Collision& /*c*/) {}
    virtual void OnCollisionExit(const Collision& /*c*/) {}
    virtual void OnTriggerEnter(GameObject& /*other*/) {}
    virtual void OnTriggerStay(GameObject& /*other*/) {}
    virtual void OnTriggerExit(GameObject& /*other*/) {}
};

} // namespace wz
