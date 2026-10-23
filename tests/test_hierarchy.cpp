#include "TestUtils.h"
#include "scene/Scene.h"

using namespace wz;

TEST(Hierarchy, ChildInheritsParentTransform) {
    Scene s;
    auto& parent = s.CreateGameObject("P").transform();
    auto& child = s.CreateGameObject("C").transform();
    parent.SetLocalPosition({10, 0, 0});
    parent.SetLocalEulerAngles({0, 90, 0});
    parent.SetLocalScale(glm::vec3(2));
    ASSERT_TRUE(child.SetParent(&parent, false));
    child.SetLocalPosition({1, 0, 0});
    ExpectVecNear(child.GetPosition(), {10, 0, -2});
    ExpectVecNear(child.GetLossyScale(), glm::vec3(2));
    ExpectMatNear(child.GetLocalToWorldMatrix(), parent.GetLocalToWorldMatrix() * child.GetLocalMatrix());
}

TEST(Hierarchy, SetParentWorldPositionStays) {
    Scene s;
    auto& parent = s.CreateGameObject("P").transform();
    auto& child = s.CreateGameObject("C").transform();
    parent.SetLocalPosition({5, 0, 0});
    parent.SetLocalEulerAngles({0, 45, 0});
    child.SetPosition({1, 2, 3});
    child.SetLocalEulerAngles({10, 20, 30});
    const glm::mat4 before = child.GetLocalToWorldMatrix();
    ASSERT_TRUE(child.SetParent(&parent, true));
    ExpectMatNear(child.GetLocalToWorldMatrix(), before);
    ASSERT_TRUE(child.SetParent(nullptr, true));
    ExpectMatNear(child.GetLocalToWorldMatrix(), before);
}

TEST(Hierarchy, SetParentWithoutWorldStaysKeepsLocal) {
    Scene s;
    auto& parent = s.CreateGameObject("P").transform();
    auto& child = s.CreateGameObject("C").transform();
    parent.SetLocalPosition({5, 0, 0});
    child.SetLocalPosition({1, 0, 0});
    child.SetParent(&parent, false);
    ExpectVecNear(child.GetLocalPosition(), {1, 0, 0});
    ExpectVecNear(child.GetPosition(), {6, 0, 0});
}

TEST(Hierarchy, ChildrenListAndRoot) {
    Scene s;
    auto& a = s.CreateGameObject("A").transform();
    auto& b = s.CreateGameObject("B").transform();
    auto& c = s.CreateGameObject("C").transform();
    b.SetParent(&a);
    c.SetParent(&b);
    EXPECT_EQ(a.GetChildCount(), 1u);
    EXPECT_EQ(a.GetChild(0), &b);
    EXPECT_EQ(c.Root(), &a);
    EXPECT_TRUE(c.IsChildOf(&a));
    EXPECT_TRUE(c.IsChildOf(&c));
    EXPECT_FALSE(a.IsChildOf(&c));
    c.SetParent(&a);
    EXPECT_EQ(b.GetChildCount(), 0u);
    EXPECT_EQ(a.GetChildCount(), 2u);
}

TEST(Hierarchy, RejectsCycles) {
    Scene s;
    auto& a = s.CreateGameObject("A").transform();
    auto& b = s.CreateGameObject("B").transform();
    b.SetParent(&a);
    EXPECT_FALSE(a.SetParent(&b));
    EXPECT_FALSE(a.SetParent(&a));
    EXPECT_EQ(a.GetParent(), nullptr);
}

TEST(Hierarchy, SetWorldPositionUnderRotatedParent) {
    Scene s;
    auto& parent = s.CreateGameObject("P").transform();
    auto& child = s.CreateGameObject("C").transform();
    parent.SetLocalPosition({3, 1, 0});
    parent.SetLocalEulerAngles({0, 90, 0});
    parent.SetLocalScale({2, 1, 1});
    child.SetParent(&parent, false);
    child.SetPosition({7, 7, 7});
    ExpectVecNear(child.GetPosition(), {7, 7, 7});
}

TEST(Hierarchy, DestroyParentDestroysChildren) {
    Scene s;
    auto& a = s.CreateGameObject("A");
    auto& b = s.CreateGameObject("B");
    auto& keep = s.CreateGameObject("Keep");
    b.transform().SetParent(&a.transform());
    keep.transform().SetParent(&a.transform());
    keep.transform().SetParent(nullptr);
    s.Destroy(a);
    EXPECT_TRUE(b.IsPendingDestroy());
    s.FlushDestroyed();
    EXPECT_EQ(s.GetGameObjectCount(), 1u);
    EXPECT_EQ(s.Find("Keep"), &keep);
    EXPECT_EQ(s.Find("B"), nullptr);
}

TEST(Hierarchy, DestroyChildDetachesFromLivingParent) {
    Scene s;
    auto& a = s.CreateGameObject("A");
    auto& b = s.CreateGameObject("B");
    b.transform().SetParent(&a.transform());
    s.Destroy(b);
    s.FlushDestroyed();
    EXPECT_EQ(a.transform().GetChildCount(), 0u);
}

TEST(Hierarchy, InactiveParentDeactivatesChildren) {
    Scene s;
    auto& a = s.CreateGameObject("A");
    auto& b = s.CreateGameObject("B");
    b.transform().SetParent(&a.transform());
    a.SetActive(false);
    EXPECT_TRUE(b.IsActiveSelf());
    EXPECT_FALSE(b.IsActiveInHierarchy());
}
