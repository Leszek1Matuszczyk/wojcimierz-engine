// ============================================================
//  Wojcimierz Engine: physics/Rigidbody.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "physics/Rigidbody.h"

#include "physics/PhysicsWorld.h"
#include "scene/GameObject.h"
#include "scene/Transform.h"

namespace wz {

void Rigidbody::OnDestroy() {
    if (auto* w = PhysicsWorld::For(scene())) w->OnComponentDestroyed(*this);
    world_ = nullptr;
    body_ = kInvalidBody;
}

glm::vec3 Rigidbody::GetVelocity() const {
    return HasBody() ? world_->GetBackend().GetLinearVelocity(body_) : pendingVelocity_;
}

void Rigidbody::SetVelocity(const glm::vec3& v) {
    if (HasBody()) world_->GetBackend().SetLinearVelocity(body_, v);
    else pendingVelocity_ = v;
}

glm::vec3 Rigidbody::GetAngularVelocity() const {
    return HasBody() ? world_->GetBackend().GetAngularVelocity(body_) : pendingAngularVelocity_;
}

void Rigidbody::SetAngularVelocity(const glm::vec3& w) {
    if (HasBody()) world_->GetBackend().SetAngularVelocity(body_, w);
    else pendingAngularVelocity_ = w;
}

void Rigidbody::AddForce(const glm::vec3& f, ForceMode mode) {
    if (bodyType != BodyType::Dynamic) return; // jak w Unity: kinematyczne ignoruja sily
    // Wszystko sprowadzamy do sily albo impulsu zaleznego od masy.
    switch (mode) {
    case ForceMode::Force:
        if (HasBody()) world_->GetBackend().AddForce(body_, f); else pendingForce_ += f;
        break;
    case ForceMode::Acceleration:
        if (HasBody()) world_->GetBackend().AddForce(body_, f * world_->GetBackend().GetMass(body_));
        else pendingForce_ += f * mass;
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
    // Ustawiamy Transform; PhysicsWorld wykryje zmiane wersji i dla cial
    // kinematycznych wykona plynny MoveKinematic zamiast teleportu.
    transform().SetPosition(worldPosition);
}

void Rigidbody::MoveRotation(const glm::quat& worldRotation) { transform().SetRotation(worldRotation); }

bool Rigidbody::IsSleeping() const { return HasBody() && !world_->GetBackend().IsActive(body_); }

void Rigidbody::WakeUp() {
    if (HasBody()) world_->GetBackend().Activate(body_);
}

} // namespace wz
