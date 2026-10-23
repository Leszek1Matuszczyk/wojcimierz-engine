// =============================================================================
// Wojcimierz Engine - tests/test_transform_nonfinite.cpp
// Na chwałę czcigodnego Wojcimierza, którego mądrość
// prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// =============================================================================
// Kontrakt fallbacku Transform: odrzucona wartosc (NaN/Inf, kwaternion zerowy)
// zostawia POPRZEDNI stan. Ogolne "brak trucizny" sprawdzaja testy z tests/nonfinite (#19).
#include "core/Math.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

using namespace wz;

namespace {
const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();
}  // namespace

TEST(TransformFallback, PositionAndScaleKeepPrevious) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalPosition({1, 2, 3});
    t.SetLocalScale({2, 2, 2});
    testing::internal::CaptureStderr();
    t.SetLocalPosition({kNaN, 0, 0});
    t.SetPosition({0, kInf, 0});
    t.Translate({0, 0, -kInf});
    t.SetLocalScale({kNaN, 1, 1});
    (void)testing::internal::GetCapturedStderr();
    EXPECT_EQ(t.GetLocalPosition(), glm::vec3(1, 2, 3));
    EXPECT_EQ(t.GetLocalScale(), glm::vec3(2, 2, 2));
}

TEST(TransformFallback, RotationKeepsPreviousAndRejectsZeroQuat) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalEulerAngles({0, 90, 0});
    const glm::quat before = t.GetLocalRotation();
    testing::internal::CaptureStderr();
    t.SetLocalRotation(glm::quat(0, 0, 0, 0));
    t.SetRotation(glm::quat(0, 0, 0, 0));
    t.SetLocalRotation(glm::quat(kNaN, 0, 0, 0));
    t.SetLocalEulerAngles({kInf, 0, 0});
    t.Rotate({0, kNaN, 0});
    t.LookAt({kNaN, 0, 0});
    t.LookAt({0, 0, -5}, {0, kInf, 0});
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(t.GetLocalRotation(), before);
    EXPECT_NE(err.find("Transform.SetLocalRotation"), std::string::npos) << err;
}

TEST(TransformFallback, RotationIsNormalized) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalRotation(glm::quat(5, 0, 0, 0));
    EXPECT_NEAR(glm::length(t.GetLocalRotation()), 1.0f, 1e-6f);
    t.SetRotation(glm::quat(0, 0, 3, 0));
    EXPECT_NEAR(glm::length(t.GetLocalRotation()), 1.0f, 1e-6f);
}

TEST(TransformFallback, ZeroScaleParentDoesNotPoisonChild) {
    Scene s;
    auto& parent = s.CreateGameObject("P").transform();
    auto& child = s.CreateGameObject("C").transform();
    child.SetParent(&parent, false);
    child.SetLocalPosition({1, 1, 1});
    parent.SetLocalScale({0, 0, 0});  // macierz nieodwracalna
    testing::internal::CaptureStderr();
    child.SetPosition({5, 5, 5});
    (void)testing::internal::GetCapturedStderr();
    EXPECT_TRUE(IsFinite(child.GetLocalPosition()));
}

TEST(TransformFallback, PhysicsPoseValidatesPartsIndependently) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    const auto v0 = t.GetChangeVersion();
    testing::internal::CaptureStderr();
    t.SetWorldPoseFromPhysics({7, 8, 9}, glm::quat(kNaN, 0, 0, 0));
    (void)testing::internal::GetCapturedStderr();
    EXPECT_EQ(t.GetLocalPosition(), glm::vec3(7, 8, 9));
    EXPECT_EQ(t.GetLocalRotation(), glm::quat(1, 0, 0, 0));
    EXPECT_EQ(t.GetChangeVersion(), v0);  // fizyka nadal nie podbija wersji
}

// #24: odrzucone wywolanie nie moze podbic changeVersion ani ustawic dirty -
// inaczej fizyka widzi teleport w kazdej klatce. Wojcimierz nie lubi falszywych alarmow.
TEST(TransformVersion, RejectedSettersDoNotBumpVersionNorDirty) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalEulerAngles({10, 20, 30});
    (void)t.GetLocalToWorldMatrix();
    const auto v0 = t.GetChangeVersion();
    testing::internal::CaptureStderr();
    t.SetLocalRotation(glm::quat(kNaN, 0, 0, 0));
    t.SetLocalRotation(glm::quat(0, 0, 0, 0));
    t.SetRotation(glm::quat(0, 0, 0, 0));
    t.SetRotation(glm::quat(1, kInf, 0, 0));
    t.SetLocalPosition({kNaN, 0, 0});
    t.SetLocalScale({kInf, 1, 1});
    t.LookAt({0, 5, 0}, {0, 1, 0});  // up rownolegle do kierunku -> NaN -> odrzucone
    t.SetWorldPoseFromPhysics({kNaN, 0, 0}, glm::quat(0, 0, 0, 0));
    (void)testing::internal::GetCapturedStderr();
    EXPECT_EQ(t.GetChangeVersion(), v0);
    EXPECT_FALSE(t.IsWorldMatrixDirty());
}

TEST(TransformVersion, IdenticalValueIsNoOp) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalPosition({1, 2, 3});
    t.SetLocalScale({2, 2, 2});
    t.SetLocalRotation(glm::quat(glm::radians(glm::vec3(0, 30, 0))));
    (void)t.GetLocalToWorldMatrix();
    const auto v0 = t.GetChangeVersion();
    t.SetLocalPosition({1, 2, 3});
    t.SetLocalScale({2, 2, 2});
    t.SetLocalRotation(t.GetLocalRotation());
    t.Translate({0, 0, 0});
    EXPECT_EQ(t.GetChangeVersion(), v0);
    EXPECT_FALSE(t.IsWorldMatrixDirty());
}

TEST(TransformVersion, PhysicsPartialRejectStillAppliesGoodPart) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    (void)t.GetLocalToWorldMatrix();
    testing::internal::CaptureStderr();
    // Zly obrot i ta sama pozycja -> nic sie nie zmienia, brak dirty.
    t.SetWorldPoseFromPhysics({0, 0, 0}, glm::quat(kNaN, 0, 0, 0));
    EXPECT_FALSE(t.IsWorldMatrixDirty());
    // Dobra pozycja + zly obrot -> pozycja przyjeta, cache uniewazniony, wersja bez zmian.
    const auto v0 = t.GetChangeVersion();
    t.SetWorldPoseFromPhysics({1, 0, 0}, glm::quat(0, 0, 0, 0));
    (void)testing::internal::GetCapturedStderr();
    EXPECT_EQ(t.GetLocalPosition(), glm::vec3(1, 0, 0));
    EXPECT_TRUE(t.IsWorldMatrixDirty());
    EXPECT_EQ(t.GetChangeVersion(), v0);
}
