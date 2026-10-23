// ============================================================
//  Wojcimierz Engine: tests/physics/test_raycast.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "PhysicsTestUtils.h"

using namespace wz;
using namespace wztest;

TEST(Raycast, HitsGroundFromAbove) {
    Scene s;
    PhysicsWorld w(s);
    auto& ground = MakeGround(s);
    w.SyncFromScene();
    auto hit = w.Raycast({1, 5, 2}, {0, -1, 0});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->gameObject, &ground);
    EXPECT_NEAR(hit->distance, 5.0f, 1e-3f);
    ExpectVecNear(hit->point, {1, 0, 2}, 1e-3f);
    ExpectVecNear(hit->normal, {0, 1, 0}, 1e-3f);
}

TEST(Raycast, MissesAndRespectsMaxDistance) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    w.SyncFromScene();
    EXPECT_FALSE(w.Raycast({0, 5, 0}, {0, 1, 0}).has_value());
    EXPECT_FALSE(w.Raycast({0, 5, 0}, {0, -1, 0}, 4.0f).has_value());
    EXPECT_TRUE(w.Raycast({0, 5, 0}, {0, -2, 0}, 6.0f).has_value()); // kierunek nieznormalizowany
}

TEST(Raycast, ClosestHitAndRigidbody) {
    Scene s;
    PhysicsWorld w(s);
    MakeGround(s);
    auto& ball = MakeBall(s, {0, 2, 0}, 0.5f);
    ball.GetComponent<Rigidbody>()->useGravity = false;
    w.SyncFromScene();
    auto hit = w.Raycast({0, 10, 0}, {0, -1, 0});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->gameObject, &ball);
    EXPECT_EQ(hit->rigidbody, ball.GetComponent<Rigidbody>());
    EXPECT_NEAR(hit->distance, 7.5f, 1e-2f);
}

TEST(Raycast, TriggersIgnoredByDefault) {
    Scene s;
    PhysicsWorld w(s);
    auto& ground = MakeGround(s);
    auto& zone = s.CreateGameObject("Zone");
    zone.transform().SetPosition({0, 2, 0});
    zone.AddComponent<BoxCollider>().isTrigger = true;
    w.SyncFromScene();
    auto hit = w.Raycast({0, 5, 0}, {0, -1, 0});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->gameObject, &ground);
    auto hitT = w.Raycast({0, 5, 0}, {0, -1, 0}, 100.0f, /*hitTriggers=*/true);
    ASSERT_TRUE(hitT.has_value());
    EXPECT_EQ(hitT->gameObject, &zone);
}

TEST(Raycast, DestroyedObjectNotHit) {
    Scene s;
    PhysicsWorld w(s);
    auto& ground = MakeGround(s);
    w.SyncFromScene();
    s.Destroy(ground);
    s.FlushDestroyed();
    EXPECT_FALSE(w.Raycast({0, 5, 0}, {0, -1, 0}).has_value());
}
