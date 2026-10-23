// ============================================================
//  Wojcimierz Engine: tests/nonfinite/test_nonfinite_transform.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Kazdy publiczny setter Transform dostaje NaN / +Inf / -Inf na kazdej osi.
// Wymaganie: po wywolaniu lokalne TRS i macierz swiatowa (takze dziecka) sa skonczone.
// Nie zakladamy konkretnego fallbacku (poprzednia wartosc / domyslna) - tylko brak trucizny.
#include "NonFiniteUtils.h"
#include "scene/Scene.h"
#include "scene/Transform.h"

using namespace wz;
using namespace wztest;

namespace {

::testing::AssertionResult TransformFinite(const Transform& t) {
    if (!Finite(t.GetLocalPosition())) return ::testing::AssertionFailure() << "localPosition nieskonczona";
    if (!Finite(t.GetLocalRotation())) return ::testing::AssertionFailure() << "localRotation nieskonczona";
    if (!Finite(t.GetLocalScale())) return ::testing::AssertionFailure() << "localScale nieskonczona";
    if (!Finite(t.GetLocalToWorldMatrix())) return ::testing::AssertionFailure() << "macierz swiatowa nieskonczona";
    return ::testing::AssertionSuccess();
}

class TransformNonFinite : public ::testing::TestWithParam<BadCase> {
protected:
    void SetUp() override {
        parent_ = &scene_.CreateGameObject("Parent").transform();
        child_ = &scene_.CreateGameObject("Child").transform();
        child_->SetParent(parent_, false);
        child_->SetLocalPosition({0, 1, 0});
        parent_->SetPosition({1, 2, 3});
    }
    void ExpectClean() {
        EXPECT_TRUE(TransformFinite(*parent_));
        EXPECT_TRUE(TransformFinite(*child_)) << "trucizna rozlala sie na dziecko";
    }
    Scene scene_;
    Transform* parent_ = nullptr;
    Transform* child_ = nullptr;
};

TEST_P(TransformNonFinite, SetLocalPosition) {
    parent_->SetLocalPosition(PoisonVec(GetParam().value, GetParam().axis));
    ExpectClean();
}
TEST_P(TransformNonFinite, SetPosition) {
    parent_->SetPosition(PoisonVec(GetParam().value, GetParam().axis));
    ExpectClean();
}
TEST_P(TransformNonFinite, SetLocalScale) {
    parent_->SetLocalScale(PoisonVec(GetParam().value, GetParam().axis));
    ExpectClean();
}
TEST_P(TransformNonFinite, SetLocalEulerAngles) {
    parent_->SetLocalEulerAngles(PoisonVec(GetParam().value, GetParam().axis));
    ExpectClean();
}
TEST_P(TransformNonFinite, SetLocalRotation) {
    parent_->SetLocalRotation(PoisonQuat(GetParam().value, GetParam().axis));
    ExpectClean();
}
TEST_P(TransformNonFinite, SetLocalRotationW) {
    parent_->SetLocalRotation(PoisonQuat(GetParam().value, 3));
    ExpectClean();
}
TEST_P(TransformNonFinite, SetRotation) {
    parent_->SetRotation(PoisonQuat(GetParam().value, GetParam().axis));
    ExpectClean();
}
TEST_P(TransformNonFinite, TranslateLocal) {
    parent_->Translate(PoisonVec(GetParam().value, GetParam().axis), false);
    ExpectClean();
}
TEST_P(TransformNonFinite, TranslateWorld) {
    parent_->Translate(PoisonVec(GetParam().value, GetParam().axis), true);
    ExpectClean();
}
TEST_P(TransformNonFinite, RotateLocal) {
    parent_->Rotate(PoisonVec(GetParam().value, GetParam().axis), false);
    ExpectClean();
}
TEST_P(TransformNonFinite, RotateWorld) {
    parent_->Rotate(PoisonVec(GetParam().value, GetParam().axis), true);
    ExpectClean();
}
TEST_P(TransformNonFinite, LookAtTarget) {
    parent_->LookAt(PoisonVec(GetParam().value, GetParam().axis));
    ExpectClean();
}
TEST_P(TransformNonFinite, LookAtUp) {
    parent_->LookAt({0, 0, -10}, PoisonVec(GetParam().value, GetParam().axis, {0, 1, 0}));
    ExpectClean();
}
TEST_P(TransformNonFinite, SetWorldPoseFromPhysicsPosition) {
    parent_->SetWorldPoseFromPhysics(PoisonVec(GetParam().value, GetParam().axis), glm::quat{1, 0, 0, 0});
    ExpectClean();
}
TEST_P(TransformNonFinite, SetWorldPoseFromPhysicsRotation) {
    parent_->SetWorldPoseFromPhysics({0, 0, 0}, PoisonQuat(GetParam().value, GetParam().axis));
    ExpectClean();
}

INSTANTIATE_TEST_SUITE_P(AllSetters, TransformNonFinite, AllBadCases(), CaseName);

// Odrzucona wartosc nie moze tez zepsuc kolejnych, poprawnych wywolan.
TEST(TransformNonFiniteRecovery, ValidSetAfterRejectedOneWorks) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalPosition({std::nanf(""), 0, 0});
    t.SetLocalPosition({4, 5, 6});
    EXPECT_EQ(t.GetLocalPosition(), glm::vec3(4, 5, 6));
    EXPECT_TRUE(TransformFinite(t));
}

} // namespace
