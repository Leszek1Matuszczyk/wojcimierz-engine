// ============================================================
//  Wojcimierz Engine: physics/Collider.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "physics/Collider.h"

#include "physics/IPhysicsBackend.h"
#include "physics/PhysicsWorld.h"
#include "physics/detail/Finite.h"

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

// Na chwałę czcigodnego Wojcimierza: kształt z NaN/inf wysadziłby Jolta, więc
// każde pole z gameplayu oczyszczamy (fallback = wartość domyślna pola).
using physics_detail::SanitizeFloat;
using physics_detail::SanitizeVec3;

ShapeDesc BoxCollider::BuildShape(const glm::vec3& lossyScale) const {
    const glm::vec3 sc = SanitizeVec3(lossyScale, glm::vec3(1.0f));
    ShapeDesc d;
    d.kind = ShapeDesc::Kind::Box;
    d.halfExtents = glm::max(Abs(SanitizeVec3(size, glm::vec3(1.0f)) * sc) * 0.5f, glm::vec3(kMinExtent));
    d.center = SanitizeVec3(center, glm::vec3(0.0f)) * sc;
    return d;
}

ShapeDesc SphereCollider::BuildShape(const glm::vec3& lossyScale) const {
    ShapeDesc d;
    d.kind = ShapeDesc::Kind::Sphere;
    // Sfera nie znosi skali niejednorodnej - bierzemy najwieksza os (jak Unity).
    const glm::vec3 sc = SanitizeVec3(lossyScale, glm::vec3(1.0f));
    d.radius = std::max(std::abs(SanitizeFloat(radius, 0.5f)) * MaxComp(Abs(sc)), kMinExtent);
    d.center = SanitizeVec3(center, glm::vec3(0.0f)) * sc;
    return d;
}

ShapeDesc CapsuleCollider::BuildShape(const glm::vec3& lossyScale) const {
    ShapeDesc d;
    d.kind = ShapeDesc::Kind::Capsule;
    const glm::vec3 sc = SanitizeVec3(lossyScale, glm::vec3(1.0f));
    const glm::vec3 s = Abs(sc);
    d.radius = std::max(std::abs(SanitizeFloat(radius, 0.5f)) * std::max(s.x, s.z), kMinExtent);
    const float halfTotal = std::abs(SanitizeFloat(height, 2.0f)) * s.y * 0.5f;
    d.halfHeight = std::max(halfTotal - d.radius, kMinExtent);
    d.center = SanitizeVec3(center, glm::vec3(0.0f)) * sc;
    return d;
}

} // namespace wz
