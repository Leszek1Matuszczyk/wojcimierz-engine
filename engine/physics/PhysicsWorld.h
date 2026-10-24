// ============================================================
//  Wojcimierz Engine: physics/PhysicsWorld.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include <unordered_set>
#include "physics/IPhysicsBackend.h"
#include "physics/PhysicsTypes.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wz {

class Scene;
class Component;
class Collider;
class Rigidbody;

// Swiat fizyki przypiety do jednej sceny.
//
// Uzycie w petli gry (kolejnosc jak w Unity):
//   scene.FixedUpdate(dt);   // skrypty: FixedUpdate
//   physics.Step(dt);        // symulacja + OnCollision*/OnTrigger*
// albo w skrocie physics.FixedUpdate(dt), ktore robi obie rzeczy.
//
// Synchronizacja ze scena (w Step, przed symulacja):
//  * nowe Rigidbody/Collider -> tworzenie ciala,
//  * zmiana pol komponentow (masa, rozmiar, typ...) -> przebudowa ciala,
//  * Transform::GetChangeVersion() rozny od zapamietanego -> teleport
//    (dla kinematycznych: MoveKinematic),
//  * po symulacji poza cial dynamicznych i kinematycznych wraca do Transformu
//    przez SetWorldPoseFromPhysics, czyli BEZ podbijania wersji.
class PhysicsWorld {
public:
    explicit PhysicsWorld(Scene& scene, std::unique_ptr<IPhysicsBackend> backend = nullptr);
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    Scene& GetScene() const { return scene_; }
    IPhysicsBackend& GetBackend() const { return *backend_; }
    static PhysicsWorld* For(const Scene& scene);

    void SetGravity(const glm::vec3& g); // NaN/inf odrzucane z ostrzezeniem
    glm::vec3 GetGravity() const { return backend_->GetGravity(); }

    // Synchronizacja, symulacja jednego kroku i rozeslanie zdarzen kolizji.
    void Step(float fixedDeltaTime);
    // scene.FixedUpdate(dt) + Step(dt).
    void FixedUpdate(float fixedDeltaTime);
    // Sama synchronizacja sceny z fizyka (np. przed Raycastem w Update).
    void SyncFromScene();

    // Odpowiednik Physics.Raycast. 'direction' nie musi byc znormalizowany.
    std::optional<RaycastHit> Raycast(const glm::vec3& origin, const glm::vec3& direction,
                                      float maxDistance = 1e30f, bool hitTriggers = false);

    std::size_t GetBodyCount() const { return bodies_.size(); }

private:
    // Próg „drobnej korekty” pozy z gameplayu, która nie budzi śpiącego ciała (#29).
    // 1e-5 m (względnie przy |p| > 1) i 1e-3 rad: przy kształcie 1 m daje to < 1 mm,
    // czyli mniej niż penetration slop Jolta (2 cm).
    static constexpr float kTinyPoseMeters = 1e-5f;
    static constexpr float kTinyPoseRadians = 1e-3f;
    bool IsTinyPoseChange(BodyHandle body, const glm::vec3& p, const glm::quat& r) const;
    void WarnNonFinitePose(const GameObject& go);
    std::unordered_set<const GameObject*> warnedPose_;
    friend class Rigidbody;
    friend class Collider;

    struct BodyRecord {
        GameObject* go = nullptr;
        Rigidbody* rb = nullptr; // nullptr = cialo statyczne z samych colliderow
        std::vector<Collider*> colliders;
        BodyHandle handle = kInvalidBody;
        std::uint64_t signature = 0;
        std::uint32_t transformVersion = 0;
        BodyType type = BodyType::Static;
        bool sensor = false;
        bool seen = false;
    };
    struct PairState {
        int contacts = 0;
        bool sensor = false;
        bool sleeping = false; // kontakty zdjete przez Jolta, bo oba ciala zasnely
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f}; // od a do b
        glm::vec3 relativeVelocity{0.0f}; // v_a - v_b w chwili zetkniecia
    };
    using PairKey = std::pair<std::uint64_t, std::uint64_t>; // (id a, id b), a < b

    void OnComponentDestroyed(Component& c);
    void RemoveRecord(std::uint64_t id, bool notifyOthers);
    void CreateBody(BodyRecord& rec);
    std::uint64_t ComputeSignature(const BodyRecord& rec) const;
    void ProcessContacts();
    void Dispatch(std::uint64_t receiver, std::uint64_t other, const PairState& st, int phase);
    BodyRecord* FindByHandle(BodyHandle h);

    Scene& scene_;
    std::unique_ptr<IPhysicsBackend> backend_;
    std::unordered_map<std::uint64_t, BodyRecord> bodies_;      // id rekordu -> rekord
    std::unordered_map<GameObject*, std::uint64_t> byObject_;
    std::unordered_map<BodyHandle, std::uint64_t> byHandle_;
    std::map<PairKey, PairState> pairs_;
    std::uint64_t nextId_ = 1;
    float lastDt_ = 1.0f / 60.0f;
};

} // namespace wz
