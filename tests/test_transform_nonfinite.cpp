// ============================================================
//  Wojcimierz Engine: tests/test_transform_nonfinite.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Kontrakt fallbacku Transform: odrzucona wartosc (NaN/Inf, kwaternion zerowy)
// zostawia POPRZEDNI stan. Ogolne "brak trucizny" sprawdzaja testy z tests/nonfinite (#19).
#include "core/Math.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>

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

// -----------------------------------------------------------------------------
// #27: setery swiatowe z tolerancja round-tripu (kWorldRoundTripEpsilon,
// kWorldPositionRelTolerance). SetPosition(GetPosition()) i SetRotation(GetRotation())
// pod obroconym i przeskalowanym rodzicem to no-op, a obrot o 1e-6 rad i ruch
// o 1e-4 to prawdziwa zmiana. Ziarno stale (powtarzalne w CI), nadpisywalne
// zmienna srodowiskowa WZ_TEST_SEED. Wojcimierz zyczy sobie powtarzalnosci.
// -----------------------------------------------------------------------------
namespace {
std::uint32_t TestSeed() {
    if (const char* env = std::getenv("WZ_TEST_SEED"); env && *env)
        return static_cast<std::uint32_t>(std::strtoul(env, nullptr, 10));
    return 0x5EEDu;  // 24301
}
}  // namespace

TEST(TransformWorldRoundTrip, FixedParentSetSameWorldValueIsNoOp) {
    Scene s;
    auto& parent = s.CreateGameObject("P").transform();
    auto& child = s.CreateGameObject("C").transform();
    parent.SetLocalPosition({3, -2, 7});
    parent.SetLocalEulerAngles({33, -71, 12});
    parent.SetLocalScale({1.7f, 0.6f, 2.3f});
    child.SetParent(&parent, false);
    child.SetLocalPosition({0.25f, 4, -1.5f});
    child.SetLocalEulerAngles({10, 20, 30});
    (void)child.GetLocalToWorldMatrix();
    const auto v0 = child.GetChangeVersion();
    child.SetPosition(child.GetPosition());
    child.SetRotation(child.GetRotation());
    child.SetWorldPoseFromPhysics(child.GetPosition(), child.GetRotation());
    child.SetLocalEulerAngles(child.GetLocalEulerAngles());
    EXPECT_EQ(child.GetChangeVersion(), v0);
    EXPECT_FALSE(child.IsWorldMatrixDirty());

    child.SetPosition(child.GetPosition() + glm::vec3(1e-4f, 0, 0));
    EXPECT_GT(child.GetChangeVersion(), v0) << "zmiana pozycji o 1e-4 musi podbic wersje";
    const auto v1 = child.GetChangeVersion();
    child.SetRotation(glm::angleAxis(1e-4f, glm::vec3(0, 1, 0)) * child.GetRotation());
    EXPECT_GT(child.GetChangeVersion(), v1) << "obrot o 1e-4 rad musi podbic wersje";
}

TEST(TransformWorldRoundTrip, LocalSettersStayExact) {
    // Setery lokalne porownuja dokladnie - mikroruch o 1 ULP nie ginie.
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalPosition({1, 2, 3});
    const auto v0 = t.GetChangeVersion();
    t.SetLocalPosition({std::nextafter(1.0f, 2.0f), 2, 3});
    EXPECT_GT(t.GetChangeVersion(), v0);
}

TEST(TransformWorldRoundTrip, RandomHierarchiesSeparateRoundTripFromMicroRotation) {
    // Wymaganie testerow (#27): na losowych hierarchiach obrot o 1e-6 rad wokol
    // losowej osi ZAWSZE podbija wersje (0 polknietych), a SetRotation(GetRotation())
    // nigdy. Polowa przypadkow ma rodzica i dziadka ze skala (2, 0.5, 3).
    const std::uint32_t seed = TestSeed();
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> pos(-5.0f, 5.0f), ang(-180.0f, 180.0f), scl(0.5f, 2.0f), u(-1.0f, 1.0f);
    std::uniform_int_distribution<int> axisPick(0, 2);
    constexpr int kCases = 20000;
    int noOpBumped = 0, repeatBumped = 0, microSwallowed = 0, rotateSwallowed = 0, moveSwallowed = 0;
    int firstBad = -1;
    float worstMicro = 1.0f;
    for (int i = 0; i < kCases; ++i) {
        const bool fixedScale = (i % 2) == 0;
        Scene s;
        auto& g = s.CreateGameObject("G").transform();
        auto& p = s.CreateGameObject("P").transform();
        auto& t = s.CreateGameObject("T").transform();
        auto scale = [&] { return fixedScale ? glm::vec3(2.0f, 0.5f, 3.0f) : glm::vec3(scl(rng), scl(rng), scl(rng)); };
        g.SetLocalPosition({pos(rng), pos(rng), pos(rng)});
        g.SetLocalEulerAngles({ang(rng), ang(rng), ang(rng)});
        g.SetLocalScale(scale());
        p.SetParent(&g, false);
        p.SetLocalPosition({pos(rng), pos(rng), pos(rng)});
        p.SetLocalEulerAngles({ang(rng), ang(rng), ang(rng)});
        p.SetLocalScale(scale());
        t.SetParent(&p, false);
        t.SetLocalPosition({pos(rng), pos(rng), pos(rng)});
        t.SetLocalEulerAngles({ang(rng), ang(rng), ang(rng)});
        (void)t.GetLocalToWorldMatrix();
        bool bad = false;

        // 1) Ta sama wartosc swiatowa - no-op.
        auto v = t.GetChangeVersion();
        t.SetPosition(t.GetPosition());
        t.SetRotation(t.GetRotation());
        t.SetWorldPoseFromPhysics(t.GetPosition(), t.GetRotation());
        t.SetLocalEulerAngles(t.GetLocalEulerAngles());
        t.Rotate({0, 0, 0});
        if (t.GetChangeVersion() != v || t.IsWorldMatrixDirty()) { ++noOpBumped; bad = true; }

        // 2) Ponowny zapis tej samej zadanej wartosci (round-trip swiat -> lokal -> swiat).
        const glm::quat q = glm::normalize(glm::quat(u(rng), u(rng), u(rng), u(rng)));
        const glm::vec3 w(pos(rng) * 4.0f, pos(rng) * 4.0f, pos(rng) * 4.0f);  // +-20
        t.SetRotation(q);
        t.SetPosition(w);
        v = t.GetChangeVersion();
        t.SetRotation(q);
        t.SetPosition(w);
        t.SetWorldPoseFromPhysics(w, q);
        if (t.GetChangeVersion() != v) { ++repeatBumped; bad = true; }

        // 3) Celowy obrot o 1e-6 rad wokol losowej osi - zawsze zmiana.
        const glm::quat before = t.GetRotation();
        const glm::vec3 axis = glm::normalize(glm::vec3(u(rng), u(rng), u(rng)) + glm::vec3(0, 0, 1e-3f));
        const glm::quat target = glm::angleAxis(1e-6f, axis) * before;
        worstMicro = std::min(worstMicro, RotationMaxDelta(glm::normalize(target), before));
        v = t.GetChangeVersion();
        t.SetRotation(target);
        if (t.GetChangeVersion() == v) { ++microSwallowed; bad = true; }

        // 4) Rotate lokalny o 1e-6 rad wokol losowej osi ukladu - tez zmiana.
        glm::vec3 e(0.0f);
        e[axisPick(rng)] = glm::degrees(1e-6f);
        v = t.GetChangeVersion();
        t.Rotate(e);
        if (t.GetChangeVersion() == v) { ++rotateSwallowed; bad = true; }

        // 5) Ruch o 1e-4 wzdluz losowej osi swiata - zawsze zmiana.
        glm::vec3 d(0.0f);
        d[axisPick(rng)] = (i % 3 == 0) ? -1e-4f : 1e-4f;
        v = t.GetChangeVersion();
        t.SetPosition(t.GetPosition() + d);
        if (t.GetChangeVersion() == v) { ++moveSwallowed; bad = true; }

        if (bad && firstBad < 0) firstBad = i;
    }
    SCOPED_TRACE(::testing::Message() << "seed=" << seed << " pierwszy zly przypadek=" << firstBad
                                      << " (powtorz: WZ_TEST_SEED=" << seed << ")");
    EXPECT_EQ(noOpBumped, 0) << "SetX(GetX()) podbilo wersje";
    EXPECT_EQ(repeatBumped, 0) << "ponowne SetRotation(q)/SetPosition(p) podbilo wersje";
    EXPECT_EQ(microSwallowed, 0) << "obrot o 1e-6 rad polkniety; najmniejsza delta=" << worstMicro;
    EXPECT_EQ(rotateSwallowed, 0) << "Rotate o 1e-6 rad polkniety";
    EXPECT_EQ(moveSwallowed, 0) << "ruch o 1e-4 polkniety";
    EXPECT_GT(worstMicro, kWorldRoundTripEpsilon);
}

TEST(TransformWorldRoundTrip, RotateZeroAfterReparentIsNoOp) {
    // Po SetParent(nullptr, true) obrot pochodzi z dekompozycji macierzy;
    // Rotate(0) i SetLocalRotation(GetLocalRotation()) nadal nie moga nic zmienic.
    Scene s;
    auto& par = s.CreateGameObject().transform();
    auto& t = s.CreateGameObject().transform();
    par.SetLocalEulerAngles({15, -40, 5});
    par.SetLocalScale({1.5f, 1.5f, 1.5f});
    t.SetParent(&par, false);
    t.SetLocalEulerAngles({10, 20, 30});
    t.SetParent(nullptr, true);
    (void)t.GetLocalToWorldMatrix();
    const auto v0 = t.GetChangeVersion();
    t.Rotate({0, 0, 0});
    t.Rotate({0, 0, 0}, true);
    t.SetLocalRotation(t.GetLocalRotation());
    EXPECT_EQ(t.GetChangeVersion(), v0);
    EXPECT_FALSE(t.IsWorldMatrixDirty());
}
