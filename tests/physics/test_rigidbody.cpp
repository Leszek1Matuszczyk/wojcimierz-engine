// ============================================================
//  Wojcimierz Engine: tests/physics/test_rigidbody.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "PhysicsTestUtils.h"
#include <cmath>
#include <limits>
#include <string>

using namespace wz;
using namespace wztest;

TEST(Rigidbody, FreeFallMatchesGravity) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 100, 0});
    Simulate(w, 60); // 1 s
    // y = y0 - g t^2 / 2 = 100 - 4.905 (semi-implicit Euler daje lekko wiecej)
    EXPECT_NEAR(ball.transform().GetPosition().y, 100.0f - 4.905f, 0.1f);
    EXPECT_NEAR(ball.GetComponent<Rigidbody>()->GetVelocity().y, -9.81f, 0.05f);
}

TEST(Rigidbody, BallComesToRestOnGround) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {0, 5, 0}, 0.5f);
    Simulate(w, 240); // 4 s
    EXPECT_NEAR(ball.transform().GetPosition().y, 0.5f, kRestTolerance);
    EXPECT_LT(glm::length(ball.GetComponent<Rigidbody>()->GetVelocity()), 0.05f);
    EXPECT_EQ(w.GetBodyCount(), 2u);
}

TEST(Rigidbody, NoGravityStaysPut) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 3, 0});
    ball.GetComponent<Rigidbody>()->useGravity = false;
    Simulate(w, 60);
    ExpectVecNear(ball.transform().GetPosition(), {0, 3, 0}, 1e-4f);
}

TEST(Rigidbody, ImpulseAndVelocityChange) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 0, 0});
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->useGravity = false;
    rb->mass = 2.0f;
    rb->AddForce({4, 0, 0}, ForceMode::Impulse); // przed powstaniem ciala - odroczone
    Simulate(w, 1);
    EXPECT_NEAR(rb->GetVelocity().x, 2.0f, 1e-3f); // dv = J / m
    rb->AddForce({0, 0, 3}, ForceMode::VelocityChange);
    Simulate(w, 1);
    EXPECT_NEAR(rb->GetVelocity().z, 3.0f, 1e-3f);
    rb->SetVelocity({0, 0, 0});
    rb->AddForce({0, 5, 0}, ForceMode::Acceleration); // a = 5 przez jeden krok
    Simulate(w, 1);
    EXPECT_NEAR(rb->GetVelocity().y, 5.0f * kDt, 1e-3f);
}

TEST(Rigidbody, KinematicIgnoresGravityAndFollowsTransform) {
    Scene s;
    PhysicsWorld w(s);
    auto& k = MakeBall(s, {0, 2, 0});
    auto* rb = k.GetComponent<Rigidbody>();
    rb->bodyType = BodyType::Kinematic;
    Simulate(w, 30);
    ExpectVecNear(k.transform().GetPosition(), {0, 2, 0});
    rb->MovePosition({1, 2, 0});
    Simulate(w, 1);
    ExpectVecNear(k.transform().GetPosition(), {1, 2, 0}, 1e-3f);
}

TEST(Rigidbody, TeleportFromGameplayIsPickedUp) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {0, 0.5f, 0});
    Simulate(w, 30);
    ball.transform().SetPosition({5, 10, 5}); // podbija changeVersion -> teleport ciala
    Simulate(w, 1);
    const glm::vec3 p = ball.transform().GetPosition();
    EXPECT_NEAR(p.x, 5.0f, 1e-3f);
    EXPECT_NEAR(p.z, 5.0f, 1e-3f);
    EXPECT_GT(p.y, 9.9f);
}

TEST(Rigidbody, PhysicsWriteBackDoesNotBumpVersion) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 10, 0});
    Simulate(w, 1); // tworzy cialo
    const auto v = ball.transform().GetChangeVersion();
    const float y = ball.transform().GetPosition().y;
    Simulate(w, 10);
    EXPECT_LT(ball.transform().GetPosition().y, y);
    EXPECT_EQ(ball.transform().GetChangeVersion(), v);
}

TEST(Rigidbody, FieldChangeRebuildsBodyAndKeepsVelocity) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 0, 0});
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->useGravity = false;
    rb->SetVelocity({1, 0, 0});
    Simulate(w, 1);
    ball.GetComponent<SphereCollider>()->radius = 2.0f; // przebudowa ksztaltu
    Simulate(w, 1);
    EXPECT_NEAR(rb->GetVelocity().x, 1.0f, 1e-3f);
    EXPECT_TRUE(rb->HasBody());
}

TEST(Rigidbody, DestroyRemovesBody) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {0, 3, 0});
    Simulate(w, 1);
    EXPECT_EQ(w.GetBodyCount(), 2u);
    s.Destroy(ball);
    Simulate(w, 1); // LateUpdate nie jest wolany - FlushDestroyed recznie
    s.FlushDestroyed();
    EXPECT_EQ(w.GetBodyCount(), 1u);
    Simulate(w, 1);
    EXPECT_EQ(w.GetBodyCount(), 1u);
}

TEST(Rigidbody, DeactivatedObjectLosesBody) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 3, 0});
    Simulate(w, 1);
    EXPECT_EQ(w.GetBodyCount(), 1u);
    ball.SetActive(false);
    Simulate(w, 1);
    EXPECT_EQ(w.GetBodyCount(), 0u);
    EXPECT_FALSE(ball.GetComponent<Rigidbody>()->HasBody());
}

TEST(Rigidbody, ChildBodyUnderTransformedParent) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& parent = s.CreateGameObject("Parent");
    parent.transform().SetPosition({5, 0, 0});
    auto& ball = MakeBall(s, {0, 0, 0});
    ball.transform().SetParent(&parent.transform(), false);
    ball.transform().SetLocalPosition({0, 3, 0});
    Simulate(w, 240);
    ExpectVecNear(ball.transform().GetPosition(), {5, 0.5f, 0}, kRestTolerance);
}

TEST(Rigidbody, ScaledBoxRestsAtScaledHeight) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& box = s.CreateGameObject("Box");
    box.transform().SetPosition({0, 3, 0});
    box.transform().SetLocalScale({1, 2, 1}); // wysokosc 2 -> srodek na y = 1
    box.AddComponent<BoxCollider>();
    box.AddComponent<Rigidbody>();
    Simulate(w, 240);
    EXPECT_NEAR(box.transform().GetPosition().y, 1.0f, kRestTolerance);
}

TEST(Rigidbody, FreezeRotationKeepsOrientation) {
    Scene s;
    PhysicsWorld w(s);
    auto& b = MakeBall(s, {0, 0, 0});
    auto* rb = b.GetComponent<Rigidbody>();
    rb->useGravity = false;
    rb->freezeRotation = true;
    Simulate(w, 1);
    rb->SetAngularVelocity({0, 5, 0});
    Simulate(w, 30);
    EXPECT_NEAR(glm::abs(glm::dot(b.transform().GetRotation(), glm::quat(1, 0, 0, 0))), 1.0f, 1e-4f);
}

// Issue #16: NaN/inf z gameplayu odrzucany, ostrzezenie tylko raz na cialo.
// Na chwałę czcigodnego Wojcimierza, strażnika skończonych liczb.
TEST(Rigidbody, NonFiniteMoveAndTorqueRejectedWarnOnce) {
    Scene s;
    PhysicsWorld w(s);
    auto& ball = MakeBall(s, {0, 5, 0});
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->useGravity = false;
    Simulate(w, 1);
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ::testing::internal::CaptureStderr();
    rb->MovePosition({inf, 0, 0});
    rb->MoveRotation(glm::quat(nan, 0, 0, 0));
    rb->AddTorque({0, nan, 0});
    for (int i = 0; i < 10; ++i) rb->AddForce({nan, 0, 0});
    const std::string log = ::testing::internal::GetCapturedStderr();
    Simulate(w, 5);
    const glm::vec3 p = ball.transform().GetPosition();
    EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
    const glm::quat q = ball.transform().GetRotation();
    EXPECT_TRUE(std::isfinite(q.w) && std::isfinite(q.x));
    const glm::vec3 av = rb->GetAngularVelocity();
    EXPECT_TRUE(std::isfinite(av.x) && std::isfinite(av.y) && std::isfinite(av.z));
    std::size_t count = 0;
    for (std::size_t at = log.find("NaN/inf"); at != std::string::npos; at = log.find("NaN/inf", at + 1)) ++count;
    EXPECT_EQ(count, 1u) << log;
}

// Pola z NaN/inf (testy z #19). Na chwałę czcigodnego Wojcimierza.
TEST(Rigidbody, NonFiniteFieldsAndGravityAreSanitized) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Scene s;
    PhysicsWorld w(s);
    w.SetGravity({0, nan, 0});
    EXPECT_FLOAT_EQ(w.GetGravity().y, -9.81f);
    auto& ball = MakeBall(s, {0, 5, 0});
    auto* rb = ball.GetComponent<Rigidbody>();
    rb->mass = nan;
    rb->linearDamping = nan;
    rb->angularDamping = -std::numeric_limits<float>::infinity();
    ball.GetComponent<SphereCollider>()->radius = nan;
    Simulate(w, 10);
    const glm::vec3 p = ball.transform().GetPosition();
    EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
    EXPECT_LT(p.y, 5.0f); // spada normalnie
}
