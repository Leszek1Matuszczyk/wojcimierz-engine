// =============================================================================
// Wojcimierz Engine - tests/test_change_tracking.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "TestUtils.h"
#include "scene/Scene.h"

using namespace wz;

namespace {
struct Chain {
    Scene s;
    Transform* a;
    Transform* b;
    Transform* c;
    Chain() {
        a = &s.CreateGameObject("A").transform();
        b = &s.CreateGameObject("B").transform();
        c = &s.CreateGameObject("C").transform();
        b->SetParent(a, false);
        c->SetParent(b, false);
    }
};
} // namespace

TEST(ChangeTracking, LocalChangeBumpsVersion) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    const auto v0 = t.GetChangeVersion();
    t.SetLocalPosition({1, 0, 0});
    EXPECT_GT(t.GetChangeVersion(), v0);
    const auto v1 = t.GetChangeVersion();
    t.SetLocalRotation(glm::quat(glm::radians(glm::vec3(0, 10, 0))));
    EXPECT_GT(t.GetChangeVersion(), v1);
    const auto v2 = t.GetChangeVersion();
    t.SetLocalScale({2, 2, 2});
    EXPECT_GT(t.GetChangeVersion(), v2);
}

TEST(ChangeTracking, VersionPropagatesToAllDescendants) {
    Chain ch;
    const auto va = ch.a->GetChangeVersion(), vb = ch.b->GetChangeVersion(), vc = ch.c->GetChangeVersion();
    ch.a->SetLocalPosition({0, 5, 0});
    EXPECT_GT(ch.a->GetChangeVersion(), va);
    EXPECT_GT(ch.b->GetChangeVersion(), vb);
    EXPECT_GT(ch.c->GetChangeVersion(), vc);
}

TEST(ChangeTracking, ChildChangeDoesNotBumpParent) {
    Chain ch;
    const auto va = ch.a->GetChangeVersion(), vb = ch.b->GetChangeVersion();
    ch.c->Translate({1, 0, 0});
    EXPECT_EQ(ch.a->GetChangeVersion(), va);
    EXPECT_EQ(ch.b->GetChangeVersion(), vb);
}

TEST(ChangeTracking, ReparentBumpsVersion) {
    Chain ch;
    const auto vc = ch.c->GetChangeVersion();
    ch.c->SetParent(ch.a);
    EXPECT_GT(ch.c->GetChangeVersion(), vc);
}

TEST(ChangeTracking, ReadingDoesNotBumpVersion) {
    Chain ch;
    ch.a->SetLocalPosition({1, 2, 3});
    const auto vc = ch.c->GetChangeVersion();
    (void)ch.c->GetLocalToWorldMatrix();
    (void)ch.c->GetPosition();
    (void)ch.c->Forward();
    EXPECT_EQ(ch.c->GetChangeVersion(), vc);
}

TEST(ChangeTracking, WorldMatrixCacheIsInvalidatedOnSubtree) {
    Chain ch;
    (void)ch.c->GetLocalToWorldMatrix();
    EXPECT_FALSE(ch.c->IsWorldMatrixDirty());
    EXPECT_FALSE(ch.a->IsWorldMatrixDirty());
    ch.a->SetLocalPosition({3, 0, 0});
    EXPECT_TRUE(ch.a->IsWorldMatrixDirty());
    EXPECT_TRUE(ch.b->IsWorldMatrixDirty());
    EXPECT_TRUE(ch.c->IsWorldMatrixDirty());
    ExpectVecNear(ch.c->GetPosition(), {3, 0, 0}); // swieza wartosc mimo cache
    EXPECT_FALSE(ch.c->IsWorldMatrixDirty());
    EXPECT_FALSE(ch.b->IsWorldMatrixDirty());      // rodzic przeliczony przy okazji
}

TEST(ChangeTracking, CachedMatrixMatchesFreshComputation) {
    Chain ch;
    ch.a->SetLocalEulerAngles({0, 30, 0});
    ch.b->SetLocalPosition({1, 0, 0});
    ch.c->SetLocalScale({2, 2, 2});
    const glm::mat4 first = ch.c->GetLocalToWorldMatrix();
    ExpectMatNear(first, ch.a->GetLocalMatrix() * ch.b->GetLocalMatrix() * ch.c->GetLocalMatrix());
    ch.b->SetLocalPosition({2, 0, 0});
    ExpectMatNear(ch.c->GetLocalToWorldMatrix(),
                  ch.a->GetLocalMatrix() * ch.b->GetLocalMatrix() * ch.c->GetLocalMatrix());
}

TEST(ChangeTracking, PhysicsWriteBackDoesNotBumpVersion) {
    Chain ch;
    (void)ch.c->GetLocalToWorldMatrix();
    const auto vb = ch.b->GetChangeVersion(), vc = ch.c->GetChangeVersion();
    const glm::quat rot(glm::radians(glm::vec3(0, 45, 0)));
    ch.b->SetWorldPoseFromPhysics({4, 5, 6}, rot);
    EXPECT_EQ(ch.b->GetChangeVersion(), vb);
    EXPECT_EQ(ch.c->GetChangeVersion(), vc);
    // ...ale cache jest uniewazniony i poza jest poprawna
    EXPECT_TRUE(ch.c->IsWorldMatrixDirty());
    ExpectVecNear(ch.b->GetPosition(), {4, 5, 6});
    ExpectVecNear(ch.c->GetPosition(), {4, 5, 6});
    EXPECT_NEAR(glm::abs(glm::dot(ch.b->GetRotation(), rot)), 1.0f, 1e-5f);
}

TEST(ChangeTracking, PhysicsWriteBackUnderTransformedParent) {
    Chain ch;
    ch.a->SetLocalPosition({10, 0, 0});
    ch.a->SetLocalEulerAngles({0, 90, 0});
    ch.b->SetWorldPoseFromPhysics({1, 2, 3}, glm::quat(1, 0, 0, 0));
    ExpectVecNear(ch.b->GetPosition(), {1, 2, 3});
    EXPECT_NEAR(glm::abs(glm::dot(ch.b->GetRotation(), glm::quat(1, 0, 0, 0))), 1.0f, 1e-5f);
}
