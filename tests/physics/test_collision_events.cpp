// ============================================================
//  Wojcimierz Engine: tests/physics/test_collision_events.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "PhysicsTestUtils.h"

#include <string>
#include <vector>

using namespace wz;
using namespace wztest;

namespace {
class Recorder final : public Component, public CollisionListener {
public:
    std::vector<std::string> log;
    Collision lastEnter;
    void OnCollisionEnter(const Collision& c) override { log.push_back("enter:" + c.gameObject->GetName()); lastEnter = c; }
    void OnCollisionStay(const Collision&) override { ++stays; }
    void OnCollisionExit(const Collision& c) override { log.push_back("exit:" + c.gameObject->GetName()); }
    void OnTriggerEnter(GameObject& o) override { log.push_back("tenter:" + o.GetName()); }
    void OnTriggerStay(GameObject&) override { ++triggerStays; }
    void OnTriggerExit(GameObject& o) override { log.push_back("texit:" + o.GetName()); }
    int stays = 0;
    int triggerStays = 0;
};
} // namespace

TEST(CollisionEvents, EnterStayExit) {
    Scene s;
    PhysicsWorld w(s);
    auto& ground = MakeGround(s);
    auto& groundRec = ground.AddComponent<Recorder>();
    auto& ball = MakeBall(s, {0, 2, 0});
    auto& rec = ball.AddComponent<Recorder>();

    Simulate(w, 120);
    ASSERT_EQ(rec.log.size(), 1u);
    EXPECT_EQ(rec.log[0], "enter:Ground");
    ASSERT_EQ(groundRec.log.size(), 1u);
    EXPECT_EQ(groundRec.log[0], "enter:Ball");
    EXPECT_GT(rec.stays, 0);
    // Normalna od podlogi do pilki wskazuje w gore, punkt styku przy y = 0.
    EXPECT_GT(rec.lastEnter.normal.y, 0.9f);
    EXPECT_NEAR(rec.lastEnter.point.y, 0.0f, 0.05f);
    EXPECT_LT(rec.lastEnter.relativeVelocity.y, -1.0f); // pilka spadala na podloge
    EXPECT_EQ(rec.lastEnter.rigidbody, nullptr);      // podloga jest statyczna

    ball.transform().SetPosition({0, 10, 0}); // teleport w gore
    Simulate(w, 2);
    ASSERT_EQ(rec.log.size(), 2u);
    EXPECT_EQ(rec.log[1], "exit:Ground");
    EXPECT_EQ(groundRec.log.back(), "exit:Ball");
}

TEST(CollisionEvents, TriggerEnterExit) {
    Scene s;
    PhysicsWorld w(s);
    auto& zone = s.CreateGameObject("Zone");
    zone.transform().SetPosition({0, 0, 0});
    auto& zc = zone.AddComponent<BoxCollider>();
    zc.size = {4, 1, 4};
    zc.isTrigger = true;
    auto& zoneRec = zone.AddComponent<Recorder>();

    auto& ball = MakeBall(s, {0, 3, 0}, 0.25f);
    auto& rec = ball.AddComponent<Recorder>();

    Simulate(w, 120); // przelatuje przez strefe (trigger nie zatrzymuje)
    EXPECT_LT(ball.transform().GetPosition().y, -3.0f);
    ASSERT_EQ(rec.log.size(), 2u);
    EXPECT_EQ(rec.log[0], "tenter:Zone");
    EXPECT_EQ(rec.log[1], "texit:Zone");
    ASSERT_EQ(zoneRec.log.size(), 2u);
    EXPECT_EQ(zoneRec.log[0], "tenter:Ball");
    EXPECT_EQ(zoneRec.log[1], "texit:Ball");
}

TEST(CollisionEvents, DeactivationSendsExitToOther) {
    Scene s;
    PhysicsWorld w(s);
    auto& ground = MakeGround(s);
    auto& groundRec = ground.AddComponent<Recorder>();
    auto& ball = MakeBall(s, {0, 0.5f, 0});
    Simulate(w, 10);
    ASSERT_EQ(groundRec.log.size(), 1u);
    ball.SetActive(false);
    Simulate(w, 1);
    ASSERT_EQ(groundRec.log.size(), 2u);
    EXPECT_EQ(groundRec.log[1], "exit:Ball");
}

TEST(CollisionEvents, DestroyedObjectDoesNotCrash) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {0, 0.5f, 0});
    ball.AddComponent<Recorder>();
    Simulate(w, 10);
    s.Destroy(ball);
    s.FlushDestroyed();
    Simulate(w, 10); // Jolt zglosi usuniete kontakty - musza zostac zignorowane
    EXPECT_EQ(w.GetBodyCount(), 1u);
}

TEST(CollisionEvents, SleepAndWakeDoNotResendEnter) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {0, 0.5f, 0});
    auto& rec = ball.AddComponent<Recorder>();
    Simulate(w, 180); // pilka lezy i zasypia
    EXPECT_TRUE(ball.GetComponent<Rigidbody>()->IsSleeping());
    ASSERT_EQ(rec.log.size(), 1u);
    // Lekkie pchniecie po podlodze: budzi cialo, ale kontakt trwa.
    ball.GetComponent<Rigidbody>()->AddForce({0.5f, 0, 0}, ForceMode::VelocityChange);
    Simulate(w, 30);
    ASSERT_EQ(rec.log.size(), 1u) << rec.log.back();
    EXPECT_EQ(rec.log[0], "enter:Ground");
}
