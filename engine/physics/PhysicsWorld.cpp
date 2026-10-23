// ============================================================
//  Wojcimierz Engine: physics/PhysicsWorld.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "physics/PhysicsWorld.h"
#include "physics/detail/Finite.h"

#include "physics/Collider.h"
#include "physics/Rigidbody.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"
#include "scene/Transform.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace wz {

namespace {
// Rejestr swiat: scena -> PhysicsWorld (komponenty znajduja swiat w OnDestroy).
std::mutex gRegistryMutex;
std::unordered_map<const Scene*, PhysicsWorld*> gRegistry;

struct Hasher {
    std::uint64_t h = 1469598103934665603ull; // FNV-1a
    template <typename T>
    void Add(const T& v) {
        unsigned char bytes[sizeof(T)];
        std::memcpy(bytes, &v, sizeof(T));
        for (unsigned char b : bytes) { h ^= b; h *= 1099511628211ull; }
    }
};

enum Phase { kEnter = 0, kStay = 1, kExit = 2 };
} // namespace

PhysicsWorld::PhysicsWorld(Scene& scene, std::unique_ptr<IPhysicsBackend> backend)
    : scene_(scene), backend_(backend ? std::move(backend) : CreateJoltBackend()) {
    std::lock_guard lock(gRegistryMutex);
    gRegistry[&scene_] = this;
}

PhysicsWorld::~PhysicsWorld() {
    {
        std::lock_guard lock(gRegistryMutex);
        auto it = gRegistry.find(&scene_);
        if (it != gRegistry.end() && it->second == this) gRegistry.erase(it);
    }
    // Nie dotykamy sceny (moze juz nie zyc) - tylko odpinamy Rigidbody, ktore znamy.
    for (auto& [id, rec] : bodies_) {
        if (rec.rb && rec.rb->world_ == this) { rec.rb->world_ = nullptr; rec.rb->body_ = kInvalidBody; }
        backend_->DestroyBody(rec.handle);
    }
}

PhysicsWorld* PhysicsWorld::For(const Scene& scene) {
    std::lock_guard lock(gRegistryMutex);
    auto it = gRegistry.find(&scene);
    return it == gRegistry.end() ? nullptr : it->second;
}

std::uint64_t PhysicsWorld::ComputeSignature(const BodyRecord& rec) const {
    // Wszystko, czego zmiana wymaga przebudowy ciala w Jolcie.
    Hasher hs;
    const glm::vec3 scale = rec.go->transform().GetLossyScale();
    hs.Add(rec.rb != nullptr);
    if (rec.rb) {
        hs.Add(rec.rb->mass);
        hs.Add(rec.rb->bodyType);
        hs.Add(rec.rb->useGravity);
        hs.Add(rec.rb->linearDamping);
        hs.Add(rec.rb->angularDamping);
        hs.Add(rec.rb->freezeRotation);
        hs.Add(rec.rb->collisionDetection);
    }
    for (const Collider* c : rec.colliders) {
        const ShapeDesc d = c->BuildShape(scale);
        hs.Add(d.kind);
        hs.Add(d.halfExtents);
        hs.Add(d.radius);
        hs.Add(d.halfHeight);
        hs.Add(d.center);
        hs.Add(c->isTrigger);
        hs.Add(c->friction);
        hs.Add(c->restitution);
    }
    return hs.h;
}

void PhysicsWorld::WarnNonFinitePose(const GameObject& go) {
    if (warnedPose_.insert(&go).second)
        std::fprintf(stderr, "[Wojcimierz] physics: '%s': Transform z NaN/inf - poza ciala nie zmieniona\n",
                     go.GetName().c_str());
}

void PhysicsWorld::SetGravity(const glm::vec3& g) {
    if (!physics_detail::IsFinite(g)) {
        std::fprintf(stderr, "[Wojcimierz] physics: SetGravity dostal NaN/inf - odrzucone\n");
        return;
    }
    backend_->SetGravity(g);
}

void PhysicsWorld::CreateBody(BodyRecord& rec) {
    Transform& t = rec.go->transform();
    const glm::vec3 scale = t.GetLossyScale();
    BodyDesc d;
    d.type = rec.rb ? rec.rb->bodyType : BodyType::Static;
    // Niezaufany Transform z gameplayu: NaN/inf nie trafia do Jolta (cialo startuje
    // w zerze, a zapis pozy po kroku przywraca skonczony Transform).
    d.position = physics_detail::IsFinite(t.GetPosition()) ? t.GetPosition() : glm::vec3(0.0f);
    d.rotation = physics_detail::IsFinite(t.GetRotation()) ? t.GetRotation() : glm::quat(1, 0, 0, 0);
    bool allTriggers = true;
    for (const Collider* c : rec.colliders) {
        d.shapes.push_back(c->BuildShape(scale));
        allTriggers = allTriggers && c->isTrigger;
    }
    // Jolt ma jeden material na cialo - bierzemy pierwszy collider.
    d.friction = std::max(physics_detail::SanitizeFloat(rec.colliders.front()->friction, 0.6f), 0.0f);
    d.restitution = std::clamp(physics_detail::SanitizeFloat(rec.colliders.front()->restitution, 0.0f), 0.0f, 1.0f);
    // Uproszczenie MVP: cialo jest triggerem, gdy WSZYSTKIE jego collidery sa triggerami.
    d.isSensor = allTriggers;
    d.userData = 0;
    if (Rigidbody* rb = rec.rb) {
        // Masa <= 0 albo NaN/inf -> 1 kg; tlumienia NaN/inf -> domyslne, ujemne -> 0.
        const float m = physics_detail::SanitizeFloat(rb->mass, 1.0f);
        d.mass = m > 0.0f ? m : 1.0f;
        d.linearDamping = std::max(physics_detail::SanitizeFloat(rb->linearDamping, 0.0f), 0.0f);
        d.angularDamping = std::max(physics_detail::SanitizeFloat(rb->angularDamping, 0.05f), 0.0f);
        d.gravityFactor = rb->useGravity ? 1.0f : 0.0f;
        d.freezeRotation = rb->freezeRotation;
        d.continuousCollision = rb->collisionDetection == CollisionDetection::Continuous;
        d.linearVelocity = rb->pendingVelocity_;
        d.angularVelocity = rb->pendingAngularVelocity_;
    }
    rec.handle = backend_->CreateBody(d);
    rec.type = d.type;
    rec.sensor = d.isSensor;
    rec.transformVersion = t.GetChangeVersion();
    rec.signature = ComputeSignature(rec);

    if (Rigidbody* rb = rec.rb; rb && rec.handle != kInvalidBody) {
        rb->world_ = this;
        rb->body_ = rec.handle;
        if (rb->bodyType == BodyType::Dynamic) {
            if (rb->pendingForce_ != glm::vec3(0.0f)) backend_->AddForce(rec.handle, rb->pendingForce_);
            if (rb->pendingImpulse_ != glm::vec3(0.0f)) backend_->AddImpulse(rec.handle, rb->pendingImpulse_);
        }
        rb->pendingForce_ = rb->pendingImpulse_ = rb->pendingVelocity_ = rb->pendingAngularVelocity_ = glm::vec3(0.0f);
    }
}

void PhysicsWorld::RemoveRecord(std::uint64_t id, bool notifyOthers) {
    auto it = bodies_.find(id);
    if (it == bodies_.end()) return;

    // Pary kontaktow z tym cialem: drugiej stronie wysylamy Exit (opcjonalnie).
    std::vector<std::pair<std::uint64_t, PairState>> exits;
    for (auto p = pairs_.begin(); p != pairs_.end();) {
        if (p->first.first == id || p->first.second == id) {
            const std::uint64_t other = p->first.first == id ? p->first.second : p->first.first;
            if (notifyOthers) exits.emplace_back(other, p->second);
            p = pairs_.erase(p);
        } else {
            ++p;
        }
    }

    // Exit wysylamy, poki rekord jeszcze istnieje (Dispatch potrzebuje GameObjectu "tego drugiego").
    for (auto& [other, st] : exits) Dispatch(other, id, st, kExit);
    it = bodies_.find(id); // callback mogl zmienic mape
    if (it == bodies_.end()) return;

    BodyRecord rec = std::move(it->second);
    bodies_.erase(it);
    byObject_.erase(rec.go);
    if (rec.handle != kInvalidBody) {
        byHandle_.erase(rec.handle);
        backend_->DestroyBody(rec.handle);
    }
    if (rec.rb && rec.rb->world_ == this) { rec.rb->world_ = nullptr; rec.rb->body_ = kInvalidBody; }
}

void PhysicsWorld::OnComponentDestroyed(Component& c) {
    auto it = byObject_.find(&c.gameObject());
    if (it == byObject_.end()) return;
    // GameObject jest wlasnie niszczony - nie wolamy callbackow w trakcie niszczenia sceny.
    RemoveRecord(it->second, /*notifyOthers=*/false);
}

void PhysicsWorld::SyncFromScene() {
    for (auto& [id, rec] : bodies_) rec.seen = false;

    for (std::size_t i = 0; i < scene_.GetGameObjectCount(); ++i) {
        GameObject& go = scene_.GetGameObjectAt(i);
        if (go.IsPendingDestroy() || !go.IsActiveInHierarchy()) continue;

        std::vector<Collider*> colliders;
        for (Collider* c : go.GetComponents<Collider>())
            if (c->enabled) colliders.push_back(c);
        if (colliders.empty()) continue; // brak ksztaltu = brak ciala

        Rigidbody* rb = go.GetComponent<Rigidbody>();
        auto found = byObject_.find(&go);
        if (found == byObject_.end()) {
            const std::uint64_t id = nextId_++;
            BodyRecord& rec = bodies_[id];
            rec.go = &go;
            rec.rb = rb;
            rec.colliders = std::move(colliders);
            rec.seen = true;
            CreateBody(rec);
            byObject_[&go] = id;
            if (rec.handle != kInvalidBody) byHandle_[rec.handle] = id;
            continue;
        }

        const std::uint64_t id = found->second;
        BodyRecord& rec = bodies_.at(id);
        rec.seen = true;
        Transform& t = go.transform();
        const bool compositionChanged = rec.rb != rb || rec.colliders != colliders;
        rec.rb = rb;
        rec.colliders = std::move(colliders);

        if (compositionChanged || ComputeSignature(rec) != rec.signature) {
            // Przebudowa: zachowujemy predkosc, pozostale stany startuja od nowa.
            if (rb && rb->HasBody()) {
                rb->pendingVelocity_ = backend_->GetLinearVelocity(rb->body_);
                rb->pendingAngularVelocity_ = backend_->GetAngularVelocity(rb->body_);
            }
            if (rec.handle != kInvalidBody) {
                byHandle_.erase(rec.handle);
                backend_->DestroyBody(rec.handle);
            }
            CreateBody(rec);
            if (rec.handle != kInvalidBody) byHandle_[rec.handle] = id;
            continue;
        }

        // Na chwałę czcigodnego Wojcimierza, który nauczył nas, że porównanie dwóch
        // liczb jest tańsze od tysiąca zdarzeń: Transform ruszony z gameplayu.
        if (t.GetChangeVersion() != rec.transformVersion && rec.handle != kInvalidBody) {
            if (!physics_detail::IsFinite(t.GetPosition()) || !physics_detail::IsFinite(t.GetRotation())) {
                // NaN/inf z gameplayu: cialo zostaje na ostatniej dobrej pozie, a zapis
                // po kroku naprawi Transform.
                WarnNonFinitePose(*rec.go);
            } else if (rec.type == BodyType::Kinematic)
                backend_->MoveKinematic(rec.handle, t.GetPosition(), t.GetRotation(), lastDt_);
            else
                backend_->SetPose(rec.handle, t.GetPosition(), t.GetRotation());
            rec.transformVersion = t.GetChangeVersion();
        }
    }

    // Obiekty, ktore zniknely ze sceny, zostaly wylaczone albo stracily collidery.
    std::vector<std::uint64_t> gone;
    for (auto& [id, rec] : bodies_)
        if (!rec.seen) gone.push_back(id);
    for (std::uint64_t id : gone) RemoveRecord(id, /*notifyOthers=*/true);
}

void PhysicsWorld::Step(float fixedDeltaTime) {
    if (fixedDeltaTime > 0.0f) lastDt_ = fixedDeltaTime;
    SyncFromScene();

    // Ciala aktywne przed krokiem tez musza oddac poze (moga zasnac w tym kroku).
    std::vector<std::uint64_t> moving;
    for (auto& [id, rec] : bodies_)
        if (rec.type != BodyType::Static && rec.handle != kInvalidBody && backend_->IsActive(rec.handle))
            moving.push_back(id);

    backend_->Step(fixedDeltaTime);

    for (auto& [id, rec] : bodies_)
        if (rec.type != BodyType::Static && rec.handle != kInvalidBody && backend_->IsActive(rec.handle))
            moving.push_back(id);
    std::sort(moving.begin(), moving.end());
    moving.erase(std::unique(moving.begin(), moving.end()), moving.end());

    // Na chwałę czcigodnego Wojcimierza: wynik symulacji wraca do Transformu
    // bez podbijania wersji, by fizyka nie pomyliła własnego dzieła z teleportem.
    for (std::uint64_t id : moving) {
        auto it = bodies_.find(id);
        if (it == bodies_.end()) continue;
        BodyRecord& rec = it->second;
        glm::vec3 p;
        glm::quat r;
        backend_->GetPose(rec.handle, p, r);
        Transform& t = rec.go->transform();
        t.SetWorldPoseFromPhysics(p, r);
        rec.transformVersion = t.GetChangeVersion();
    }

    ProcessContacts();
}

void PhysicsWorld::FixedUpdate(float fixedDeltaTime) {
    scene_.FixedUpdate(fixedDeltaTime);
    Step(fixedDeltaTime);
}

PhysicsWorld::BodyRecord* PhysicsWorld::FindByHandle(BodyHandle h) {
    auto it = byHandle_.find(h);
    if (it == byHandle_.end()) return nullptr;
    auto rec = bodies_.find(it->second);
    return rec == bodies_.end() ? nullptr : &rec->second;
}

void PhysicsWorld::ProcessContacts() {
    std::vector<PairKey> entered;
    std::vector<PairKey> woke;
    std::vector<std::pair<PairKey, PairState>> exited;

    for (const ContactEvent& e : backend_->DrainContactEvents()) {
        auto ia = byHandle_.find(e.a), ib = byHandle_.find(e.b);
        if (ia == byHandle_.end() || ib == byHandle_.end()) continue; // cialo juz usuniete
        std::uint64_t a = ia->second, b = ib->second;
        glm::vec3 normal = e.normal;
        glm::vec3 relVel = e.relativeVelocity;
        if (a > b) { std::swap(a, b); normal = -normal; relVel = -relVel; }
        const PairKey key{a, b};

        if (e.type == ContactEvent::Type::Added) {
            PairState& st = pairs_[key];
            if (st.sleeping) {
                // Obudzone ciala nadal sie stykaja - kontynuacja, nie nowe Enter.
                st.sleeping = false;
                ++st.contacts;
                woke.push_back(key);
            } else if (st.contacts++ == 0) {
                st.point = e.point;
                st.normal = normal;
                st.relativeVelocity = relVel;
                st.sensor = e.sensor;
                entered.push_back(key);
            }
        } else {
            auto it = pairs_.find(key);
            if (it == pairs_.end()) continue;
            if (--it->second.contacts <= 0) {
                it->second.contacts = 0;
                if (e.sleeping) {
                    it->second.sleeping = true; // para trwa, tylko spi
                } else {
                    exited.emplace_back(key, it->second);
                    pairs_.erase(it);
                }
            }
        }
    }

    // Uspiona para, w ktorej ktores cialo sie obudzilo, a Jolt nie odtworzyl kontaktu,
    // oznacza, ze ciala sie rozdzielily -> Exit.
    for (auto it = pairs_.begin(); it != pairs_.end();) {
        PairState& st = it->second;
        if (st.sleeping && std::find(woke.begin(), woke.end(), it->first) == woke.end()) {
            const BodyRecord* ra = nullptr;
            const BodyRecord* rb = nullptr;
            if (auto a = bodies_.find(it->first.first); a != bodies_.end()) ra = &a->second;
            if (auto b = bodies_.find(it->first.second); b != bodies_.end()) rb = &b->second;
            const bool awake = (ra && ra->handle != kInvalidBody && backend_->IsActive(ra->handle)) ||
                               (rb && rb->handle != kInvalidBody && backend_->IsActive(rb->handle));
            if (awake) {
                exited.emplace_back(it->first, st);
                it = pairs_.erase(it);
                continue;
            }
        }
        ++it;
    }

    // Enter -> Stay (pary trwajace, bez tych, ktore weszly w tym kroku) -> Exit.
    // Kopiujemy stany, bo callbacki moga zmieniac swiat.
    std::vector<std::pair<PairKey, PairState>> stays;
    for (const auto& [key, st] : pairs_)
        if (!st.sleeping && std::find(entered.begin(), entered.end(), key) == entered.end()) stays.emplace_back(key, st);
    std::vector<std::pair<PairKey, PairState>> enters;
    for (const PairKey& key : entered)
        if (auto it = pairs_.find(key); it != pairs_.end()) enters.emplace_back(key, it->second);

    auto both = [&](const PairKey& k, const PairState& st, int phase) {
        Dispatch(k.first, k.second, st, phase);
        Dispatch(k.second, k.first, st, phase);
    };
    for (auto& [k, st] : enters) both(k, st, kEnter);
    for (auto& [k, st] : stays) both(k, st, kStay);
    for (auto& [k, st] : exited) both(k, st, kExit);
}

void PhysicsWorld::Dispatch(std::uint64_t receiver, std::uint64_t other, const PairState& st, int phase) {
    auto ir = bodies_.find(receiver);
    if (ir == bodies_.end()) return;
    GameObject* self = ir->second.go;
    if (!self || self->IsPendingDestroy()) return;

    auto io = bodies_.find(other);
    GameObject* otherGo = io != bodies_.end() ? io->second.go : nullptr;
    Rigidbody* otherRb = io != bodies_.end() ? io->second.rb : nullptr;
    if (!otherGo) return; // drugie cialo juz nie istnieje

    Collision col;
    if (!st.sensor) {
        col.gameObject = otherGo;
        col.rigidbody = otherRb;
        col.point = st.point;
        // st.normal wskazuje od nizszego id do wyzszego; chcemy od "drugiego" do odbiorcy.
        col.normal = receiver < other ? -st.normal : st.normal;
        // Predkosc wzgledna z chwili zetkniecia (po solverze bylaby juz ~0).
        col.relativeVelocity = receiver < other ? st.relativeVelocity : -st.relativeVelocity;
    }

    // Kopia listy komponentow: callback moze dodac nowe.
    std::vector<CollisionListener*> listeners;
    for (std::size_t i = 0; i < self->GetComponentCount(); ++i) {
        Component& c = self->GetComponentAt(i);
        if (!c.enabled) continue;
        if (auto* l = dynamic_cast<CollisionListener*>(&c)) listeners.push_back(l);
    }
    for (CollisionListener* l : listeners) {
        if (st.sensor) {
            if (phase == kEnter) l->OnTriggerEnter(*otherGo);
            else if (phase == kStay) l->OnTriggerStay(*otherGo);
            else l->OnTriggerExit(*otherGo);
        } else {
            if (phase == kEnter) l->OnCollisionEnter(col);
            else if (phase == kStay) l->OnCollisionStay(col);
            else l->OnCollisionExit(col);
        }
    }
}

std::optional<RaycastHit> PhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                                bool hitTriggers) {
    auto hit = backend_->Raycast(origin, direction, maxDistance, hitTriggers);
    if (!hit) return std::nullopt;
    BodyRecord* rec = FindByHandle(hit->body);
    if (!rec) return std::nullopt;
    RaycastHit out;
    out.gameObject = rec->go;
    out.rigidbody = rec->rb;
    out.point = hit->point;
    out.normal = hit->normal;
    out.distance = hit->distance;
    return out;
}

} // namespace wz
