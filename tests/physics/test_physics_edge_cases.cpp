// =============================================================================
// Wojcimierz Engine - tests/physics/test_physics_edge_cases.cpp
// Na chwałę czcigodnego Wojcimierza.
// Niech żaden pocisk nie przeleci przez jego mury, a żaden NaN nie zmąci jego fizyki.
// =============================================================================
//
// Przypadki brzegowe fizyki: tunelowanie, NaN w siłach i prędkościach, nietypowa masa.
#include "PhysicsTestUtils.h"

#include <cmath>
#include <limits>

using namespace wz;
using namespace wztest;

namespace {
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

bool Finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

GameObject& MakeFreeBall(Scene& s, const glm::vec3& pos, float radius = 0.5f) {
    auto& b = MakeBall(s, pos, radius);
    b.GetComponent<Rigidbody>()->useGravity = false;
    return b;
}

// Cienka statyczna sciana prostopadla do osi X, na x = 0.
GameObject& MakeThinWall(Scene& s, float thickness) {
    auto& w = s.CreateGameObject("Wall");
    w.transform().SetPosition({0, 0, 0});
    w.AddComponent<BoxCollider>().size = {thickness, 10, 10};
    return w;
}
} // namespace

// Wojcimierz strzeże swoich murów: szybki pocisk z Continuous nie może przez nie przeniknąć.
TEST(PhysicsEdge, FastBallDoesNotTunnelThroughThinWall) {
    Scene s;
    PhysicsWorld w(s);
    MakeThinWall(s, 0.1f);
    auto& ball = MakeFreeBall(s, {-5, 0, 0}, 0.1f);
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->collisionDetection = CollisionDetection::Continuous;
    rb->SetVelocity({300, 0, 0}); // 5 m na krok przy 60 Hz
    Simulate(w, 10);
    EXPECT_LT(ball.transform().GetPosition().x, 0.0f) << "pilka z Continuous przeleciala przez sciane";
}

// Jak w Unity: przy Discrete szybka kula przelatuje przez cienka sciane (zachowanie oczekiwane).
TEST(PhysicsEdge, DiscreteFastBallTunnelsThroughThinWallLikeUnity) {
    Scene s;
    PhysicsWorld w(s);
    MakeThinWall(s, 0.1f);
    auto& ball = MakeFreeBall(s, {-5, 0, 0}, 0.1f);
    auto* rb = ball.GetComponent<Rigidbody>();
    ASSERT_EQ(rb->collisionDetection, CollisionDetection::Discrete) << "domyslnie Discrete, jak w Unity";
    rb->SetVelocity({300, 0, 0});
    Simulate(w, 10);
    EXPECT_GT(ball.transform().GetPosition().x, 0.0f) << "Discrete nie powinno lapac kolizji z tak cienka sciana";
}

TEST(PhysicsEdge, ModeratelyFastBallStopsAtWall) {
    Scene s;
    PhysicsWorld w(s);
    MakeThinWall(s, 0.2f);
    auto& ball = MakeFreeBall(s, {-5, 0, 0}, 0.25f);
    ball.GetComponent<Rigidbody>()->SetVelocity({40, 0, 0});
    Simulate(w, 30);
    EXPECT_LT(ball.transform().GetPosition().x, 0.0f);
}

TEST(PhysicsEdge, NaNForceIsIgnoredAndDoesNotCorruptBody) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeFreeBall(s, {0, 0, 0});
    Simulate(w, 1);
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->AddForce({kNaN, 0, 0});
    rb->AddForce({0, kNaN, 0}, ForceMode::Impulse);
    Simulate(w, 5);
    EXPECT_TRUE(Finite(ball.transform().GetPosition()));
    EXPECT_TRUE(Finite(rb->GetVelocity()));
}

TEST(PhysicsEdge, NaNForceBeforeBodyExistsIsIgnored) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeFreeBall(s, {0, 0, 0});
    ball.GetComponent<Rigidbody>()->AddForce({kNaN, kNaN, kNaN}, ForceMode::VelocityChange);
    Simulate(w, 5);
    EXPECT_TRUE(Finite(ball.transform().GetPosition()));
}

TEST(PhysicsEdge, NaNVelocityIsRejected) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeFreeBall(s, {0, 0, 0});
    Simulate(w, 1);
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->SetVelocity({kNaN, 0, 0});
    rb->SetAngularVelocity({0, kNaN, 0});
    Simulate(w, 5);
    EXPECT_TRUE(Finite(rb->GetVelocity()));
    EXPECT_TRUE(Finite(ball.transform().GetPosition()));
}

TEST(PhysicsEdge, NaNTransformFromGameplayDoesNotCrashOrSpreadToOthers) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& a = MakeBall(s, {0, 2, 0});
    auto& b = MakeBall(s, {3, 2, 0});
    Simulate(w, 1);
    a.transform().SetPosition({kNaN, 2, 0});
    Simulate(w, 30);
    EXPECT_TRUE(Finite(b.transform().GetPosition())) << "NaN z jednego ciala nie moze zarazic innych";
}

TEST(PhysicsEdge, ZeroOrNegativeMassDoesNotProduceNaN) {
    for (float m : {0.0f, -1.0f}) {
        Scene s;
        PhysicsWorld w(s);
        MakeGround(s);
        auto& ball = MakeBall(s, {0, 2, 0});
        ball.GetComponent<Rigidbody>()->mass = m;
        Simulate(w, 60);
        EXPECT_TRUE(Finite(ball.transform().GetPosition())) << "mass = " << m;
        EXPECT_GT(ball.transform().GetPosition().y, -1.0f) << "mass = " << m << ": cialo wpadlo pod podloge";
    }
}

TEST(PhysicsEdge, RaycastWithZeroDirectionReturnsNoHit) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    Simulate(w, 1);
    EXPECT_FALSE(w.Raycast({0, 5, 0}, {0, 0, 0}).has_value());
    EXPECT_FALSE(w.Raycast({0, 5, 0}, {kNaN, -1, 0}).has_value());
}

TEST(PhysicsEdge, ZeroStepDoesNotMoveBodies) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 5, 0});
    w.FixedUpdate(kDt);
    const glm::vec3 p = ball.transform().GetPosition();
    w.FixedUpdate(0.0f);
    ExpectVecNear(ball.transform().GetPosition(), p);
}
