// ============================================================
//  Wojcimierz Engine: tests/physics/test_rest.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// #29: kula leżąca na płaskiej podłodze ma leżeć, nawet gdy coś ją budzi.
#include "PhysicsTestUtils.h"

#include <glm/glm.hpp>

using namespace wz;
using namespace wztest;

// Obudzona kula (Activate co klatkę) nie może sama się toczyć. Przyczyną był cache
// kontaktów par ciał w Jolt (jrouwe/JoltPhysics#1686), który backend teraz wyłącza.
TEST(PhysicsRest, AwakeBallOnFlatGroundDoesNotDrift) {
    for (int rot = 0; rot < 2; ++rot)
        for (float x : {0.0f, 3.7f}) {
            SCOPED_TRACE(testing::Message() << "rot=" << rot << " x=" << x);
            Scene s;
            PhysicsWorld w(s);
            MakeGround(s);
            auto& ball = MakeBall(s, {x, 0.5f, 0});
            if (rot) ball.transform().LookAt({5, 3, -4});
            auto* rb = ball.GetComponent<Rigidbody>();
            Simulate(w, 120);
            const glm::vec3 p0 = ball.transform().GetPosition();
            float vmax = 0;
            for (int f = 0; f < 600; ++f) {
                rb->WakeUp();
                w.FixedUpdate(kDt);
                vmax = std::max(vmax, glm::length(rb->GetVelocity()));
            }
            // Przed poprawką: do 4 cm/s i 0.15-0.2 m w 10 s.
            EXPECT_LT(vmax, 1e-3f);
            EXPECT_LT(glm::length(ball.transform().GetPosition() - p0), 1e-2f);
        }
}

// LookAt co klatkę na leżącej kuli: drobna korekta pozy nie budzi ciała, więc kula zasypia.
// Przed poprawką SetPose(Activate) szedł w 223/240 klatek i kula nie zasnęła ani razu.
TEST(PhysicsRest, LookAtEveryFrameLetsRestingBallSleep) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {3.7f, 0.5f, 0});
    auto* rb = ball.GetComponent<Rigidbody>();
    Simulate(w, 120);
    const glm::vec3 p0 = ball.transform().GetPosition();
    int sleeping = 0;
    for (int f = 0; f < 240; ++f) {
        ball.transform().LookAt({5, 3, -4});
        w.FixedUpdate(kDt);
        sleeping += rb->IsSleeping();
    }
    EXPECT_GE(sleeping, 180); // pierwsza duża zmiana budzi, potem zasypia po ~0.5 s
    EXPECT_TRUE(rb->IsSleeping());
    EXPECT_LT(glm::length(ball.transform().GetPosition() - p0), 1e-3f);
}

// Duża zmiana pozy z gameplayu dalej budzi śpiące ciało (teleport w powietrze -> spada).
TEST(PhysicsRest, LargePoseChangeStillWakesBody) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {0, 0.5f, 0});
    auto* rb = ball.GetComponent<Rigidbody>();
    Simulate(w, 120);
    ASSERT_TRUE(rb->IsSleeping());
    ball.transform().SetPosition({0, 3, 0});
    Simulate(w, 30);
    EXPECT_LT(ball.transform().GetPosition().y, 2.9f);
}
