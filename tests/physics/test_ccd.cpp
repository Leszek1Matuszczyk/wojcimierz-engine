// ============================================================
//  Wojcimierz Engine: tests/physics/test_ccd.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Issue #15: ciagle wykrywanie kolizji (CollisionDetection::Continuous).
#include "PhysicsTestUtils.h"

using namespace wz;
using namespace wztest;

namespace {
float ShootAtWall(CollisionDetection mode, bool switchAfterCreate = false) {
    Scene s;
    PhysicsWorld w(s);
    auto& wall = s.CreateGameObject("Wall");
    wall.AddComponent<BoxCollider>().size = {0.1f, 10, 10};
    auto& ball = MakeBall(s, {-5, 0, 0}, 0.1f);
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->useGravity = false;
    if (!switchAfterCreate) rb->collisionDetection = mode;
    rb->SetVelocity({300, 0, 0}); // 5 m na krok przy 60 Hz
    if (switchAfterCreate) {
        w.FixedUpdate(0.0f); // cialo powstaje jako Discrete
        rb->collisionDetection = mode; // zmiana pola przebudowuje cialo z zachowaniem predkosci
    }
    Simulate(w, 10);
    return ball.transform().GetPosition().x;
}
} // namespace

TEST(PhysicsCcd, DiscreteTunnelsAt300ms) {
    // Dokumentuje, po co jest Continuous: domyslny tryb przepuszcza pocisk.
    EXPECT_GT(ShootAtWall(CollisionDetection::Discrete), 0.0f);
}

TEST(PhysicsCcd, ContinuousStopsAt300ms) {
    EXPECT_LT(ShootAtWall(CollisionDetection::Continuous), 0.0f);
}

TEST(PhysicsCcd, SwitchingToContinuousAtRuntimeWorks) {
    EXPECT_LT(ShootAtWall(CollisionDetection::Continuous, true), 0.0f);
}
