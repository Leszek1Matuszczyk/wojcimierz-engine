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
#include <cstdio>

#include "scene/GameObject.h"

namespace wz {

void Collider::OnDestroy() {
    if (auto* w = PhysicsWorld::For(scene())) w->OnComponentDestroyed(*this);
}

namespace {
glm::vec3 Abs(const glm::vec3& v) { return glm::abs(v); }
float MaxComp(const glm::vec3& v) { return std::max(v.x, std::max(v.y, v.z)); }
} // namespace

void Collider::WarnExtent(const char* what, double v) const {
    clippedThisBuild_ = true;
    if (warnedExtent_) return;
    warnedExtent_ = true;
    std::fprintf(stderr, "[Wojcimierz] physics: %s collidera '%s' po skali = %g poza zakresem [%g, %g] m - przycinam\n",
                 what, gameObject().GetName().c_str(), v, double(kMinShapeExtent), double(kMaxShapeExtent));
}

float Collider::LimitExtent(double scaled, const char* what) const {
    const double a = std::abs(scaled);
    if (a > kMaxShapeExtent) { WarnExtent(what, a); return kMaxShapeExtent; }
    // Zero albo mikrowymiar to częsty przypadek, więc nie ostrzegamy.
    // Uwaga: tak mały kształt spoczywa do ~2 cm w podłodze. To nie convex radius (przy boxie
    // jest już min(domyślny, połowa wymiaru), a kula i kapsuła go nie mają), tylko
    // penetration slop Jolta (PhysicsSettings::mPenetrationSlop = 0.02 m), czyli dopuszczalne
    // wzajemne zagłębienie ciał. Wojcimierz wybacza te dwa centymetry.
    if (a < kMinShapeExtent) return kMinShapeExtent;
    return static_cast<float>(a);
}

float Collider::LimitOffset(double scaled, const char* what) const {
    if (std::abs(scaled) > kMaxShapeExtent) {
        WarnExtent(what, scaled);
        return scaled > 0 ? kMaxShapeExtent : -kMaxShapeExtent;
    }
    return static_cast<float>(scaled);
}

// Na chwałę czcigodnego Wojcimierza: kształt z NaN/inf wysadziłby Jolta, więc
// każde pole z gameplayu oczyszczamy (fallback = wartość domyślna pola).
using physics_detail::SanitizeFloat;
using physics_detail::SanitizeVec3;

glm::vec3 Collider::CenterOf(const glm::vec3& sc) const {
    const glm::vec3 c = SanitizeVec3(center, glm::vec3(0.0f));
    return {LimitOffset(double(c.x) * sc.x, "center"), LimitOffset(double(c.y) * sc.y, "center"),
            LimitOffset(double(c.z) * sc.z, "center")};
}

ShapeDesc BoxCollider::BuildShape(const glm::vec3& lossyScale) const {
    const glm::vec3 sc = SanitizeVec3(lossyScale, glm::vec3(1.0f));
    ShapeDesc d;
    BeginLimits();
    d.kind = ShapeDesc::Kind::Box;
    const glm::vec3 sz = SanitizeVec3(size, glm::vec3(1.0f));
    for (int i = 0; i < 3; ++i) d.halfExtents[i] = LimitExtent(double(sz[i]) * sc[i] * 0.5, "size");
    d.center = CenterOf(sc);
    EndLimits();
    return d;
}

ShapeDesc SphereCollider::BuildShape(const glm::vec3& lossyScale) const {
    ShapeDesc d;
    BeginLimits();
    d.kind = ShapeDesc::Kind::Sphere;
    // Sfera nie znosi skali niejednorodnej - bierzemy najwieksza os (jak Unity).
    const glm::vec3 sc = SanitizeVec3(lossyScale, glm::vec3(1.0f));
    d.radius = LimitExtent(double(SanitizeFloat(radius, 0.5f)) * MaxComp(Abs(sc)), "radius");
    d.center = CenterOf(sc);
    EndLimits();
    return d;
}

ShapeDesc CapsuleCollider::BuildShape(const glm::vec3& lossyScale) const {
    ShapeDesc d;
    BeginLimits();
    d.kind = ShapeDesc::Kind::Capsule;
    const glm::vec3 sc = SanitizeVec3(lossyScale, glm::vec3(1.0f));
    const glm::vec3 s = Abs(sc);
    d.radius = LimitExtent(double(SanitizeFloat(radius, 0.5f)) * std::max(s.x, s.z), "radius");
    const double halfTotal = double(LimitExtent(double(SanitizeFloat(height, 2.0f)) * s.y, "height")) * 0.5;
    d.halfHeight = LimitExtent(halfTotal - d.radius, "height");
    d.center = CenterOf(sc);
    EndLimits();
    return d;
}

} // namespace wz
