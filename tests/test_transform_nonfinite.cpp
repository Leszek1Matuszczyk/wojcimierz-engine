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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
// #27/#28: setery BEZ tolerancji. No-op tylko, gdy wejscie jest bitowo rowne
// temu, co zwraca getter (SetPosition(GetPosition()), SetRotation(GetRotation()),
// Rotate(0)); kazda inna wartosc jest zapisywana i podbija wersje - fizyka (#7)
// i audio (#9) widza zmiany tylko przez changeVersion. Ziarno stale (powtarzalne
// w CI), nadpisywalne zmienna WZ_TEST_SEED. Wojcimierz zyczy sobie powtarzalnosci.
// -----------------------------------------------------------------------------
namespace {
std::uint32_t TestSeed() {
    if (const char* env = std::getenv("WZ_TEST_SEED"); env && *env)
        return static_cast<std::uint32_t>(std::strtoul(env, nullptr, 10));
    return 0x5EEDu;  // 24301
}
bool SameBitsT(const glm::vec3& a, const glm::vec3& b) { return std::memcmp(&a, &b, sizeof a) == 0; }
bool SameBitsT(const glm::quat& a, const glm::quat& b) { return std::memcmp(&a, &b, sizeof a) == 0; }
// Rodzic obrocony i przeskalowany (2, 0.5, 3) z dzieckiem.
struct ScaledParent {
    Scene s;
    Transform& parent = s.CreateGameObject("P").transform();
    Transform& child = s.CreateGameObject("C").transform();
    ScaledParent() {
        parent.SetLocalPosition({3, -2, 7});
        parent.SetLocalEulerAngles({33, -71, 12});
        parent.SetLocalScale({2.0f, 0.5f, 3.0f});
        child.SetParent(&parent, false);
        child.SetLocalPosition({0.25f, 4, -1.5f});
        child.SetLocalEulerAngles({10, 20, 30});
        (void)child.GetLocalToWorldMatrix();
    }
};
}  // namespace

TEST(TransformWorldRoundTrip, FixedParentSetSameWorldValueIsNoOp) {
    ScaledParent h;
    Transform& child = h.child;
    const auto v0 = child.GetChangeVersion();
    child.SetPosition(child.GetPosition());
    child.SetRotation(child.GetRotation());
    child.SetRotation(-child.GetRotation());  // -q to ten sam obrot
    child.SetWorldPoseFromPhysics(child.GetPosition(), child.GetRotation());
    child.SetLocalEulerAngles(child.GetLocalEulerAngles());
    child.Rotate({0, 0, 0});
    child.Rotate({0, 0, 0}, true);
    child.Translate({0, 0, 0}, true);
    EXPECT_EQ(child.GetChangeVersion(), v0);
    EXPECT_FALSE(child.IsWorldMatrixDirty());

    child.SetPosition(child.GetPosition() + glm::vec3(1e-4f, 0, 0));
    EXPECT_GT(child.GetChangeVersion(), v0) << "zmiana pozycji o 1e-4 musi podbic wersje";
    const auto v1 = child.GetChangeVersion();
    child.SetRotation(glm::angleAxis(1e-4f, glm::vec3(0, 1, 0)) * child.GetRotation());
    EXPECT_GT(child.GetChangeVersion(), v1) << "obrot o 1e-4 rad musi podbic wersje";
}

TEST(TransformWorldRoundTrip, LocalSettersStayExact) {
    // Setery lokalne porownuja bitowo - mikroruch o 1 ULP nie ginie.
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
    int noOpBumped = 0, microSwallowed = 0, rotateSwallowed = 0, moveSwallowed = 0, tinySwallowed = 0;
    int firstBad = -1;
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

        // 1) Ta sama wartosc z gettera - dokladny no-op.
        auto v = t.GetChangeVersion();
        t.SetPosition(t.GetPosition());
        t.SetRotation(t.GetRotation());
        t.SetWorldPoseFromPhysics(t.GetPosition(), t.GetRotation());
        t.SetLocalEulerAngles(t.GetLocalEulerAngles());
        t.Rotate({0, 0, 0});
        if (t.GetChangeVersion() != v || t.IsWorldMatrixDirty()) { ++noOpBumped; bad = true; }

        // 2) Celowy obrot o 1e-6 rad wokol losowej osi - zawsze zmiana.
        const glm::vec3 axis = glm::normalize(glm::vec3(u(rng), u(rng), u(rng)) + glm::vec3(0, 0, 1e-3f));
        v = t.GetChangeVersion();
        t.SetRotation(glm::angleAxis(1e-6f, axis) * t.GetRotation());
        if (t.GetChangeVersion() == v) { ++microSwallowed; bad = true; }

        // 3) Rotate lokalny o 1e-6 rad wokol losowej osi ukladu - tez zmiana.
        glm::vec3 e(0.0f);
        e[axisPick(rng)] = glm::degrees(1e-6f);
        v = t.GetChangeVersion();
        t.Rotate(e);
        if (t.GetChangeVersion() == v) { ++rotateSwallowed; bad = true; }

        // 4) Ruch o 1e-4 wzdluz losowej osi swiata - zawsze zmiana.
        glm::vec3 d(0.0f);
        d[axisPick(rng)] = (i % 3 == 0) ? -1e-4f : 1e-4f;
        v = t.GetChangeVersion();
        t.SetPosition(t.GetPosition() + d);
        if (t.GetChangeVersion() == v) { ++moveSwallowed; bad = true; }

        // 5) Lokalny ruch o 1 ULP - zawsze zmiana (brak jakiejkolwiek tolerancji).
        glm::vec3 lp = t.GetLocalPosition();
        lp.x = std::nextafter(lp.x, 100.0f);
        v = t.GetChangeVersion();
        t.SetLocalPosition(lp);
        if (t.GetChangeVersion() == v) { ++tinySwallowed; bad = true; }

        if (bad && firstBad < 0) firstBad = i;
    }
    SCOPED_TRACE(::testing::Message() << "seed=" << seed << " pierwszy zly przypadek=" << firstBad
                                      << " (powtorz: WZ_TEST_SEED=" << seed << ")");
    EXPECT_EQ(noOpBumped, 0) << "SetX(GetX()) podbilo wersje";
    EXPECT_EQ(microSwallowed, 0) << "obrot o 1e-6 rad polkniety";
    EXPECT_EQ(rotateSwallowed, 0) << "Rotate o 1e-6 rad polkniety";
    EXPECT_EQ(moveSwallowed, 0) << "ruch o 1e-4 polkniety";
    EXPECT_EQ(tinySwallowed, 0) << "lokalny ruch o 1 ULP polkniety";
}

TEST(TransformWorldRoundTrip, TenThousandSelfAssignmentsUnderScaledParentDoNotDrift) {
    ScaledParent h;
    Transform& t = h.child;
    const glm::vec3 p0 = t.GetPosition();
    const glm::quat q0 = t.GetRotation();
    const glm::vec3 lp0 = t.GetLocalPosition();
    const glm::quat lq0 = t.GetLocalRotation();
    const auto v0 = t.GetChangeVersion();
    for (int i = 0; i < 10000; ++i) {
        t.SetPosition(t.GetPosition());
        t.SetRotation(t.GetRotation());
        t.SetWorldPoseFromPhysics(t.GetPosition(), t.GetRotation());
    }
    EXPECT_EQ(t.GetChangeVersion(), v0);
    // To dokladny no-op, wiec zero dryfu - bity sa nietkniete.
    EXPECT_TRUE(SameBitsT(t.GetPosition(), p0));
    EXPECT_TRUE(SameBitsT(t.GetRotation(), q0));
    EXPECT_TRUE(SameBitsT(t.GetLocalPosition(), lp0));
    EXPECT_TRUE(SameBitsT(t.GetLocalRotation(), lq0));
}

TEST(TransformWorldRoundTrip, SmallStepsAtThousandMetersBumpEveryStep) {
    // #28: 1000 krokow po 2 mm przy |p| = 1000. Kazdy krok to zmiana (wersja +1),
    // a wynik jest taki, jaki daje sama arytmetyka float (ULP(1000) = 6.1e-5).
    const glm::vec3 step(0.002f, 0.0f, 0.0f);
    {
        Scene s;
        auto& t = s.CreateGameObject().transform();
        t.SetPosition({1000, 0, 0});
        glm::vec3 expected(1000, 0, 0);
        for (int i = 0; i < 1000; ++i) {
            SCOPED_TRACE(::testing::Message() << "bez rodzica, krok=" << i);
            const auto v = t.GetChangeVersion();
            t.SetPosition(t.GetPosition() + step);
            expected += step;
            ASSERT_EQ(t.GetChangeVersion(), v + 1);
        }
        EXPECT_TRUE(SameBitsT(t.GetPosition(), expected));
        EXPECT_NEAR(t.GetPosition().x - 1000.0f, 2.0f, 0.02f);
    }
    {
        ScaledParent h;
        Transform& t = h.child;
        t.SetPosition({1000, 0, 0});
        const glm::vec3 start = t.GetPosition();
        float maxStepErr = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            SCOPED_TRACE(::testing::Message() << "pod rodzicem (2, 0.5, 3), krok=" << i);
            const auto v = t.GetChangeVersion();
            const glm::vec3 before = t.GetPosition();
            t.SetPosition(before + step);
            ASSERT_EQ(t.GetChangeVersion(), v + 1);
            const glm::vec3 err = glm::abs((t.GetPosition() - before) - step);
            maxStepErr = std::max({maxStepErr, err.x, err.y, err.z});
        }
        // Granica float (nie blad), na chwale czcigodnego Wojcimierza: pod rodzicem ze
        // skala 0.5 wspolrzedne lokalne siegaja ~2000, wiec kazdy krok jest kwantowany
        // do ULP(2000) = 1.2e-4 lokalnie, a skala rodzica (do 3) i obrot mieszaja to na
        // trzy osie. Zmierzone: blad kroku max 2.6e-4 (13% z 2 mm), suma 2.128 m
        // zamiast 2 m. Granica: 4.5 * ULP(max |lokalna|) + 2 * ULP(1000).
        const glm::vec3 lp = glm::abs(t.GetLocalPosition());
        const float maxLocal = std::max({lp.x, lp.y, lp.z});
        const float bound = 4.5f * (std::nextafter(maxLocal, 1e9f) - maxLocal) +
                            2.0f * (std::nextafter(1000.0f, 2000.0f) - 1000.0f);
        EXPECT_LE(maxStepErr, bound) << "zaden krok nie zginal ani nie odbiegl ponad kwantyzacje float";
        EXPECT_NEAR(t.GetPosition().x - start.x, 2.0f, 0.2f);
        EXPECT_NEAR(t.GetPosition().y, start.y, 0.2f);
        EXPECT_NEAR(t.GetPosition().z, start.z, 0.2f);
    }
}

TEST(TransformWorldRoundTrip, TinyRotationStepsBumpEveryStepAndAccumulate) {
    // #28: 100 x 1e-7 rad (Rotate lokalny i SetRotation(GetRotation() * dq)).
    // Kazdy krok podbija wersje, a suma to ~1e-5 rad.
    const glm::vec3 eul(0.0f, glm::degrees(1e-7f), 0.0f);
    const glm::quat dq = glm::angleAxis(1e-7f, glm::vec3(0, 1, 0));
    for (int mode = 0; mode < 2; ++mode) {
        Scene s;
        auto& t = s.CreateGameObject().transform();
        for (int i = 0; i < 100; ++i) {
            SCOPED_TRACE(::testing::Message() << (mode == 0 ? "Rotate" : "SetRotation(Get*dq)") << " krok=" << i);
            const auto v = t.GetChangeVersion();
            if (mode == 0) t.Rotate(eul);
            else t.SetRotation(t.GetRotation() * dq);
            ASSERT_EQ(t.GetChangeVersion(), v + 1);
        }
        const float angle = 2.0f * std::atan2(std::fabs(t.GetRotation().y), t.GetRotation().w);
        EXPECT_NEAR(angle, 1e-5f, 1e-6f) << "tryb " << mode;
    }
}

TEST(TransformWorldRoundTrip, WorldCompareSeesParentChange) {
    // Porownanie korzysta z cache macierzy swiatowej - zmiana rodzica musi go
    // uniewaznic, inaczej SetPosition(stara pozycja) byloby falszywym no-opem.
    ScaledParent h;
    Transform& t = h.child;
    const glm::vec3 oldPos = t.GetPosition();
    const glm::quat oldRot = t.GetRotation();
    h.parent.SetLocalPosition(h.parent.GetLocalPosition() + glm::vec3(1, 0, 0));
    ASSERT_FALSE(SameBitsT(t.GetPosition(), oldPos));
    auto v = t.GetChangeVersion();
    t.SetPosition(oldPos);
    EXPECT_GT(t.GetChangeVersion(), v) << "swiat sie zmienil przez rodzica - zapis musi podbic wersje";
    EXPECT_NEAR(glm::length(t.GetPosition() - oldPos), 0.0f, 1e-4f);

    h.parent.SetLocalEulerAngles({0, 90, 0});
    v = t.GetChangeVersion();
    t.SetRotation(oldRot);
    EXPECT_GT(t.GetChangeVersion(), v);
    EXPECT_LT(RotationMaxDelta(t.GetRotation(), oldRot), 1e-6f);

    // Zmiana rodzica przez SetParent tez uniewaznia porownanie.
    Transform& other = h.s.CreateGameObject("O").transform();
    other.SetLocalPosition({50, 0, 0});
    const glm::vec3 before = t.GetPosition();
    t.SetParent(&other, false);
    v = t.GetChangeVersion();
    t.SetPosition(before);
    EXPECT_GT(t.GetChangeVersion(), v);
    EXPECT_NEAR(glm::length(t.GetPosition() - before), 0.0f, 1e-4f);
}

TEST(TransformWorldRoundTrip, WorldMatrixReflectsTinySet) {
    // Najmniejsza mozliwa zmiana (1 ULP) jest zapisana i widoczna w macierzy swiatowej.
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetPosition({1000, 0, 0});
    (void)t.GetLocalToWorldMatrix();
    const float next = std::nextafter(1000.0f, 2000.0f);
    t.SetPosition({next, 0, 0});
    EXPECT_TRUE(t.IsWorldMatrixDirty());
    EXPECT_EQ(t.GetLocalToWorldMatrix()[3].x, next);
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
