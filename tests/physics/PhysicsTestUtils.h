// ============================================================
//  Wojcimierz Engine: tests/physics/PhysicsTestUtils.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "TestUtils.h"
#include "physics/Collider.h"
#include "physics/PhysicsWorld.h"
#include "physics/Rigidbody.h"
#include "scene/Scene.h"

namespace wztest {

constexpr float kDt = 1.0f / 60.0f;
// Jolt dopuszcza ~2 cm penetracji spoczynkowej (PhysicsSettings::mPenetrationSlop).
constexpr float kRestTolerance = 0.03f;

// Podloga: statyczny box 20x1x20, gorna powierzchnia na y = 0.
inline wz::GameObject& MakeGround(wz::Scene& s) {
    auto& g = s.CreateGameObject("Ground");
    g.transform().SetPosition({0, -0.5f, 0});
    g.AddComponent<wz::BoxCollider>().size = {20, 1, 20};
    return g;
}

inline wz::GameObject& MakeBall(wz::Scene& s, const glm::vec3& pos, float radius = 0.5f) {
    auto& b = s.CreateGameObject("Ball");
    b.transform().SetPosition(pos);
    b.AddComponent<wz::SphereCollider>().radius = radius;
    b.AddComponent<wz::Rigidbody>();
    return b;
}

inline void Simulate(wz::PhysicsWorld& w, int steps) {
    for (int i = 0; i < steps; ++i) w.FixedUpdate(kDt);
}

} // namespace wztest
