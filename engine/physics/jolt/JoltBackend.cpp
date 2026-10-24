// ============================================================
//  Wojcimierz Engine: physics/jolt/JoltBackend.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
//
// Implementacja IPhysicsBackend na Jolt Physics. To jedyny plik silnika,
// ktory widzi naglowki Jolta - reszta rozmawia przez IPhysicsBackend.

// Jolt.h musi byc pierwszy.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "physics/IPhysicsBackend.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>

namespace wz {
namespace {

using namespace JPH;

// --- Globalna inicjalizacja Jolta (licznik referencji: kilka swiatow naraz) ---
std::mutex gJoltInitMutex;
int gJoltInitCount = 0;

void AcquireJolt() {
    std::lock_guard lock(gJoltInitMutex);
    if (gJoltInitCount++ == 0) {
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();
    }
}

void ReleaseJolt() {
    std::lock_guard lock(gJoltInitMutex);
    if (--gJoltInitCount == 0) {
        UnregisterTypes();
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }
}

// --- Warstwy: nieruchome (statyczne) i ruchome (dynamiczne + kinematyczne) ---
namespace Layers {
constexpr ObjectLayer NON_MOVING = 0;
constexpr ObjectLayer MOVING = 1;
constexpr ObjectLayer NUM_LAYERS = 2;
} // namespace Layers

namespace BPLayers {
constexpr BroadPhaseLayer NON_MOVING(0);
constexpr BroadPhaseLayer MOVING(1);
constexpr uint NUM_LAYERS = 2;
} // namespace BPLayers

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
        // Statyczne nie zderzaja sie ze statycznymi - reszta tak.
        return !(a == Layers::NON_MOVING && b == Layers::NON_MOVING);
    }
};

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    uint GetNumBroadPhaseLayers() const override { return BPLayers::NUM_LAYERS; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        return layer == Layers::NON_MOVING ? BPLayers::NON_MOVING : BPLayers::MOVING;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
        return layer == BPLayers::NON_MOVING ? "NON_MOVING" : "MOVING";
    }
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer layer, BroadPhaseLayer bp) const override {
        return !(layer == Layers::NON_MOVING && bp == BPLayers::NON_MOVING);
    }
};

inline Vec3 ToJ(const glm::vec3& v) { return Vec3(v.x, v.y, v.z); }
inline RVec3 ToJR(const glm::vec3& v) { return RVec3(v.x, v.y, v.z); }
inline Quat ToJ(const glm::quat& q) { return Quat(q.x, q.y, q.z, q.w).Normalized(); }
inline glm::vec3 ToG(Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
inline glm::vec3 ToGR(RVec3Arg v) { return {float(v.GetX()), float(v.GetY()), float(v.GetZ())}; }
inline glm::quat ToG(QuatArg q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }
inline BodyID ToId(BodyHandle h) { return BodyID(h); }

// Kontakty przychodza z watkow roboczych Jolta - zbieramy je pod mutexem,
// a PhysicsWorld rozsyla je na watku gry po zakonczeniu kroku.
class ContactCollector final : public ContactListener {
public:
    void OnContactAdded(const Body& b1, const Body& b2, const ContactManifold& m, ContactSettings&) override {
        ContactEvent e;
        e.type = ContactEvent::Type::Added;
        e.a = b1.GetID().GetIndexAndSequenceNumber();
        e.b = b2.GetID().GetIndexAndSequenceNumber();
        const RVec3 p = m.mRelativeContactPointsOn1.empty() ? m.mBaseOffset : m.GetWorldSpaceContactPointOn1(0);
        e.point = {float(p.GetX()), float(p.GetY()), float(p.GetZ())};
        // mWorldSpaceNormal wskazuje kierunek wypchniecia ciala 2, czyli od 1 do 2.
        e.normal = {m.mWorldSpaceNormal.GetX(), m.mWorldSpaceNormal.GetY(), m.mWorldSpaceNormal.GetZ()};
        e.relativeVelocity = ToG(b1.GetLinearVelocity() - b2.GetLinearVelocity());
        e.sensor = b1.IsSensor() || b2.IsSensor();
        std::lock_guard lock(mutex_);
        events_.push_back(e);
    }

    void OnContactRemoved(const SubShapeIDPair& pair) override {
        ContactEvent e;
        e.type = ContactEvent::Type::Removed;
        e.a = pair.GetBody1ID().GetIndexAndSequenceNumber();
        e.b = pair.GetBody2ID().GetIndexAndSequenceNumber();
        std::lock_guard lock(mutex_);
        events_.push_back(e);
    }

    std::vector<ContactEvent> Drain() {
        std::lock_guard lock(mutex_);
        std::vector<ContactEvent> out;
        out.swap(events_);
        return out;
    }

private:
    std::mutex mutex_;
    std::vector<ContactEvent> events_;
};

class NoSensorFilter final : public BodyFilter {
public:
    bool ShouldCollideLocked(const Body& b) const override { return !b.IsSensor(); }
};

RefConst<Shape> MakeShape(const ShapeDesc& d) {
    switch (d.kind) {
    case ShapeDesc::Kind::Box: {
        const float minHalf = std::min(d.halfExtents.x, std::min(d.halfExtents.y, d.halfExtents.z));
        const float convex = std::min(cDefaultConvexRadius, 0.5f * minHalf);
        return new BoxShape(ToJ(d.halfExtents), convex);
    }
    case ShapeDesc::Kind::Sphere: return new SphereShape(d.radius);
    case ShapeDesc::Kind::Capsule: return new CapsuleShape(d.halfHeight, d.radius);
    }
    return new SphereShape(0.5f);
}

bool IsIdentityOffset(const ShapeDesc& d) {
    return glm::all(glm::lessThan(glm::abs(d.center), glm::vec3(1e-6f))) &&
           std::abs(std::abs(d.rotation.w) - 1.0f) < 1e-6f;
}

RefConst<Shape> MakeBodyShape(const std::vector<ShapeDesc>& shapes) {
    if (shapes.size() == 1 && IsIdentityOffset(shapes[0])) return MakeShape(shapes[0]);
    // Kilka colliderow albo przesuniety srodek -> ksztalt zlozony.
    StaticCompoundShapeSettings compound;
    for (const ShapeDesc& d : shapes) compound.AddShape(ToJ(d.center), ToJ(d.rotation), MakeShape(d));
    Shape::ShapeResult r = compound.Create();
    if (r.HasError()) return new SphereShape(0.5f);
    return r.Get();
}

struct JoltLifetime {
    JoltLifetime() { AcquireJolt(); }
    ~JoltLifetime() { ReleaseJolt(); }
};

// --- Wlasciwy backend ---
class JoltBackend final : public IPhysicsBackend {
public:
    JoltBackend() {
        temp_ = std::make_unique<TempAllocatorImpl>(16 * 1024 * 1024);
        const int hw = int(std::thread::hardware_concurrency());
        const int threads = std::clamp(hw - 1, 1, 4);
        jobs_ = std::make_unique<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers, threads);
        system_.Init(kMaxBodies, 0, kMaxBodyPairs, kMaxContactConstraints, bpInterface_, objVsBp_, objPair_);
        system_.SetContactListener(&contacts_);

        // Na chwałę czcigodnego Wojcimierza, przed którym kula leży spokojnie (#29):
        // wyłączamy cache kontaktów par ciał. Przy włączonym Jolt odtwarza kontakt z cache,
        // dopóki ciało nie przesunie się o 1 mm albo nie obróci o 2°. Błąd tego przybliżenia
        // dla brył okrągłych kumuluje się, więc obudzona kula na płaskiej podłodze sama
        // się rozpędza (zmierzone: do 4 cm/s, 0.15 m w 10 s; po wyłączeniu < 1e-5 m/s).
        // Zgłoszone u autora Jolta: jrouwe/JoltPhysics#1686. Koszt to pełna detekcja
        // kolizji par w każdym kroku.
        PhysicsSettings ps = system_.GetPhysicsSettings();
        ps.mUseBodyPairContactCache = false;
        system_.SetPhysicsSettings(ps);
    }

    ~JoltBackend() override {
        BodyInterface& bi = system_.GetBodyInterface();
        for (const BodyID& id : alive_) {
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
        alive_.clear();
        system_.SetContactListener(nullptr);
    }

    BodyHandle CreateBody(const BodyDesc& d) override {
        if (d.shapes.empty()) return kInvalidBody;
        EMotionType motion = EMotionType::Static;
        ObjectLayer layer = Layers::NON_MOVING;
        if (d.type == BodyType::Dynamic) { motion = EMotionType::Dynamic; layer = Layers::MOVING; }
        if (d.type == BodyType::Kinematic) { motion = EMotionType::Kinematic; layer = Layers::MOVING; }

        BodyCreationSettings s(MakeBodyShape(d.shapes), ToJR(d.position), ToJ(d.rotation), motion, layer);
        s.mFriction = d.friction;
        s.mRestitution = d.restitution;
        s.mLinearDamping = d.linearDamping;
        s.mAngularDamping = d.angularDamping;
        s.mGravityFactor = d.gravityFactor;
        s.mIsSensor = d.isSensor;
        s.mUserData = d.userData;
        s.mLinearVelocity = ToJ(d.linearVelocity);
        s.mAngularVelocity = ToJ(d.angularVelocity);
        if (d.isSensor && motion != EMotionType::Dynamic) s.mCollideKinematicVsNonDynamic = true;
        if (motion == EMotionType::Dynamic) {
            // Masa z komponentu, tensor bezwladnosci liczony z ksztaltu.
            s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            s.mMassPropertiesOverride.mMass = std::max(d.mass, 1e-4f);
            if (d.continuousCollision) s.mMotionQuality = EMotionQuality::LinearCast;
            if (d.freezeRotation)
                s.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        }
        const EActivation act = motion == EMotionType::Static ? EActivation::DontActivate : EActivation::Activate;
        const BodyID id = system_.GetBodyInterface().CreateAndAddBody(s, act);
        if (id.IsInvalid()) return kInvalidBody;
        alive_.push_back(id);
        return id.GetIndexAndSequenceNumber();
    }

    void DestroyBody(BodyHandle h) override {
        const BodyID id = ToId(h);
        auto it = std::find(alive_.begin(), alive_.end(), id);
        if (it == alive_.end()) return;
        *it = alive_.back();
        alive_.pop_back();
        BodyInterface& bi = system_.GetBodyInterface();
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }

    void SetPose(BodyHandle h, const glm::vec3& p, const glm::quat& r) override {
        system_.GetBodyInterface().SetPositionAndRotation(ToId(h), ToJR(p), ToJ(r), EActivation::Activate);
    }
    void SetPoseNoWake(BodyHandle h, const glm::vec3& p, const glm::quat& r) override {
        system_.GetBodyInterface().SetPositionAndRotation(ToId(h), ToJR(p), ToJ(r), EActivation::DontActivate);
    }
    void MoveKinematic(BodyHandle h, const glm::vec3& p, const glm::quat& r, float dt) override {
        system_.GetBodyInterface().MoveKinematic(ToId(h), ToJR(p), ToJ(r), dt);
    }
    void GetPose(BodyHandle h, glm::vec3& p, glm::quat& r) const override {
        RVec3 jp;
        Quat jr;
        system_.GetBodyInterface().GetPositionAndRotation(ToId(h), jp, jr);
        p = ToGR(jp);
        r = ToG(jr);
    }

    glm::vec3 GetLinearVelocity(BodyHandle h) const override { return ToG(system_.GetBodyInterface().GetLinearVelocity(ToId(h))); }
    void SetLinearVelocity(BodyHandle h, const glm::vec3& v) override {
        BodyInterface& bi = system_.GetBodyInterface();
        bi.SetLinearVelocity(ToId(h), ToJ(v));
        bi.ActivateBody(ToId(h));
    }
    glm::vec3 GetAngularVelocity(BodyHandle h) const override { return ToG(system_.GetBodyInterface().GetAngularVelocity(ToId(h))); }
    void SetAngularVelocity(BodyHandle h, const glm::vec3& w) override {
        BodyInterface& bi = system_.GetBodyInterface();
        bi.SetAngularVelocity(ToId(h), ToJ(w));
        bi.ActivateBody(ToId(h));
    }
    void AddForce(BodyHandle h, const glm::vec3& f) override { system_.GetBodyInterface().AddForce(ToId(h), ToJ(f)); }
    void AddImpulse(BodyHandle h, const glm::vec3& j) override { system_.GetBodyInterface().AddImpulse(ToId(h), ToJ(j)); }
    void AddTorque(BodyHandle h, const glm::vec3& t) override { system_.GetBodyInterface().AddTorque(ToId(h), ToJ(t)); }
    void AddAngularImpulse(BodyHandle h, const glm::vec3& j) override {
        system_.GetBodyInterface().AddAngularImpulse(ToId(h), ToJ(j));
    }
    float GetMass(BodyHandle h) const override {
        BodyLockRead lock(system_.GetBodyLockInterface(), ToId(h));
        if (!lock.Succeeded()) return 0.0f;
        const Body& b = lock.GetBody();
        if (!b.IsDynamic()) return 0.0f;
        const float inv = b.GetMotionProperties()->GetInverseMass();
        return inv > 0.0f ? 1.0f / inv : 0.0f;
    }
    bool IsActive(BodyHandle h) const override { return system_.GetBodyInterface().IsActive(ToId(h)); }
    void Activate(BodyHandle h) override { system_.GetBodyInterface().ActivateBody(ToId(h)); }

    void SetGravity(const glm::vec3& g) override { system_.SetGravity(ToJ(g)); }
    glm::vec3 GetGravity() const override { return ToG(system_.GetGravity()); }

    void Step(float dt) override {
        if (dt <= 0.0f) return;
        // Jolt zaleca krok kolizji nie dluzszy niz ~1/60 s.
        const int collisionSteps = std::max(1, int(std::ceil(dt * 60.0f - 1e-3f)));
        system_.Update(dt, collisionSteps, temp_.get(), jobs_.get());
    }

    std::vector<ContactEvent> DrainContactEvents() override {
        std::vector<ContactEvent> events = contacts_.Drain();
        const BodyInterface& bi = system_.GetBodyInterface();
        for (ContactEvent& e : events) {
            if (e.type != ContactEvent::Type::Removed) continue;
            const BodyID a = ToId(e.a), b = ToId(e.b);
            // Oba ciala nadal istnieja i oba spia (statyczne nigdy nie sa aktywne) -> to tylko sen.
            e.sleeping = bi.IsAdded(a) && bi.IsAdded(b) && !bi.IsActive(a) && !bi.IsActive(b);
        }
        return events;
    }

    std::optional<BackendRaycastHit> Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance,
                                             bool hitTriggers) const override {
        const float len = glm::length(dir);
        if (len < 1e-8f || maxDistance <= 0.0f) return std::nullopt;
        // Bardzo duze dystanse psuja precyzje float - ograniczamy do 1e6 m.
        const float dist = std::min(maxDistance, 1e6f);
        const glm::vec3 n = dir / len;
        const RRayCast ray{ToJR(origin), ToJ(n * dist)};
        ClosestHitCollisionCollector<CastRayCollector> collector;
        NoSensorFilter noSensors;
        BodyFilter all;
        system_.GetNarrowPhaseQuery().CastRay(ray, RayCastSettings(), collector, {}, {},
                                              hitTriggers ? all : static_cast<const BodyFilter&>(noSensors));
        if (!collector.HadHit()) return std::nullopt;
        const RayCastResult& hit = collector.mHit;
        BackendRaycastHit out;
        out.body = hit.mBodyID.GetIndexAndSequenceNumber();
        out.distance = hit.mFraction * dist;
        out.point = origin + n * out.distance;
        BodyLockRead lock(system_.GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded())
            out.normal = ToG(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ToJR(out.point)));
        return out;
    }

    std::size_t GetBodyCount() const override { return alive_.size(); }

private:
    static constexpr uint kMaxBodies = 65536;
    static constexpr uint kMaxBodyPairs = 65536;
    static constexpr uint kMaxContactConstraints = 16384;

    JoltLifetime life_; // pierwszy: zyje dluzej niz wszystko, co Jolt alokuje
    BPLayerInterfaceImpl bpInterface_;
    ObjectVsBroadPhaseLayerFilterImpl objVsBp_;
    ObjectLayerPairFilterImpl objPair_;
    ContactCollector contacts_;
    std::unique_ptr<TempAllocatorImpl> temp_;
    std::unique_ptr<JobSystemThreadPool> jobs_;
    mutable PhysicsSystem system_;
    std::vector<BodyID> alive_;
};

} // namespace

// Na chwałę czcigodnego Wojcimierza: jedyne miejsce, gdzie silnik wybiera backend.
// Jego mądrość przewidziała, że kiedyś obok Jolta stanie inny.
std::unique_ptr<IPhysicsBackend> CreateJoltBackend() { return std::make_unique<JoltBackend>(); }

} // namespace wz
