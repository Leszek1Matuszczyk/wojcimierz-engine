// ============================================================
//  Wojcimierz Engine: physics/Rigidbody.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "physics/Rigidbody.h"

#include "physics/PhysicsWorld.h"
#include "scene/GameObject.h"
#include "scene/Transform.h"

#include <cmath>
#include <cstdio>

namespace wz {

namespace {
bool Finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool Finite(const glm::quat& q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}
} // namespace

// Na chwałę czcigodnego Wojcimierza, który nie wpuszcza do swego świata liczb
// bez sensu: jeden NaN w Jolcie psuje cialo i jego Transform na zawsze.
bool Rigidbody::Reject(const char* what, const glm::vec3& v) {
    if (Finite(v)) return false;
    if (!warnedNonFinite_) {
        warnedNonFinite_ = true;
        std::fprintf(stderr, "[Wojcimierz] physics: Rigidbody '%s': %s dostal NaN/inf - wywolanie odrzucone "
                             "(kolejne takie ostrzezenia dla tego ciala sa wyciszone)\n",
                     gameObject().GetName().c_str(), what);
    }
    return true;
}

bool Rigidbody::Reject(const char* what, const glm::quat& q) {
    return Reject(what, glm::vec3(Finite(q) ? 0.0f : NAN));
}

void Rigidbody::OnDestroy() {
    if (auto* w = PhysicsWorld::For(scene())) w->OnComponentDestroyed(*this);
    world_ = nullptr;
    body_ = kInvalidBody;
}

glm::vec3 Rigidbody::GetVelocity() const {
    return HasBody() ? world_->GetBackend().GetLinearVelocity(body_) : pendingVelocity_;
}

void Rigidbody::SetVelocity(const glm::vec3& v) {
    if (Reject("SetVelocity", v)) return;
    if (HasBody()) world_->GetBackend().SetLinearVelocity(body_, v);
    else pendingVelocity_ = v;
}

glm::vec3 Rigidbody::GetAngularVelocity() const {
    return HasBody() ? world_->GetBackend().GetAngularVelocity(body_) : pendingAngularVelocity_;
}

void Rigidbody::SetAngularVelocity(const glm::vec3& w) {
    if (Reject("SetAngularVelocity", w)) return;
    if (HasBody()) world_->GetBackend().SetAngularVelocity(body_, w);
    else pendingAngularVelocity_ = w;
}

void Rigidbody::AddForce(const glm::vec3& f, ForceMode mode) {
    if (Reject("AddForce", f)) return;
    if (bodyType != BodyType::Dynamic) return; // jak w Unity: kinematyczne ignoruja sily
    // Wszystko sprowadzamy do sily albo impulsu zaleznego od masy.
    switch (mode) {
    case ForceMode::Force:
        if (HasBody()) world_->GetBackend().AddForce(body_, f); else pendingForce_ += f;
        break;
    case ForceMode::Acceleration:
        if (HasBody()) world_->GetBackend().AddForce(body_, f * world_->GetBackend().GetMass(body_));
        else pendingForce_ += f * (std::isfinite(mass) && mass > 0.0f ? mass : 1.0f);
        break;
    case ForceMode::Impulse:
        if (HasBody()) world_->GetBackend().AddImpulse(body_, f); else pendingImpulse_ += f;
        break;
    case ForceMode::VelocityChange:
        SetVelocity(GetVelocity() + f);
        break;
    }
}

void Rigidbody::AddTorque(const glm::vec3& t, ForceMode mode) {
    if (Reject("AddTorque", t)) return;
    if (bodyType != BodyType::Dynamic || !HasBody()) return;
    auto& b = world_->GetBackend();
    switch (mode) {
    case ForceMode::Force:
    case ForceMode::Acceleration: b.AddTorque(body_, t); break;
    case ForceMode::Impulse:
    case ForceMode::VelocityChange: b.AddAngularImpulse(body_, t); break;
    }
}

void Rigidbody::MovePosition(const glm::vec3& worldPosition) {
    if (Reject("MovePosition", worldPosition)) return;
    // Ustawiamy Transform; PhysicsWorld wykryje zmiane wersji i dla cial
    // kinematycznych wykona plynny MoveKinematic zamiast teleportu.
    transform().SetPosition(worldPosition);
}

void Rigidbody::MoveRotation(const glm::quat& worldRotation) {
    if (Reject("MoveRotation", worldRotation)) return;
    transform().SetRotation(worldRotation);
}

bool Rigidbody::IsSleeping() const { return HasBody() && !world_->GetBackend().IsActive(body_); }

void Rigidbody::WakeUp() {
    if (HasBody()) world_->GetBackend().Activate(body_);
}

} // namespace wz
