// ============================================================
//  Wojcimierz Engine: physics/Collider.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "physics/Collider.h"

#include "physics/IPhysicsBackend.h"
#include "physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>

namespace wz {

void Collider::OnDestroy() {
    if (auto* w = PhysicsWorld::For(scene())) w->OnComponentDestroyed(*this);
}

namespace {
glm::vec3 Abs(const glm::vec3& v) { return glm::abs(v); }
float MaxComp(const glm::vec3& v) { return std::max(v.x, std::max(v.y, v.z)); }
constexpr float kMinExtent = 1e-3f; // Jolt nie lubi zerowych ksztaltow
} // namespace

ShapeDesc BoxCollider::BuildShape(const glm::vec3& lossyScale) const {
    ShapeDesc d;
    d.kind = ShapeDesc::Kind::Box;
    d.halfExtents = glm::max(Abs(size * lossyScale) * 0.5f, glm::vec3(kMinExtent));
    d.center = center * lossyScale;
    return d;
}

ShapeDesc SphereCollider::BuildShape(const glm::vec3& lossyScale) const {
    ShapeDesc d;
    d.kind = ShapeDesc::Kind::Sphere;
    // Sfera nie znosi skali niejednorodnej - bierzemy najwieksza os (jak Unity).
    d.radius = std::max(std::abs(radius) * MaxComp(Abs(lossyScale)), kMinExtent);
    d.center = center * lossyScale;
    return d;
}

ShapeDesc CapsuleCollider::BuildShape(const glm::vec3& lossyScale) const {
    ShapeDesc d;
    d.kind = ShapeDesc::Kind::Capsule;
    const glm::vec3 s = Abs(lossyScale);
    d.radius = std::max(std::abs(radius) * std::max(s.x, s.z), kMinExtent);
    const float halfTotal = std::abs(height) * s.y * 0.5f;
    d.halfHeight = std::max(halfTotal - d.radius, kMinExtent);
    d.center = center * lossyScale;
    return d;
}

} // namespace wz
