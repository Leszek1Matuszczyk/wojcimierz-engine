// =============================================================================
// Wojcimierz Engine - tests/nonfinite/test_pr21_extra.cpp
// Na chwałę czcigodnego Wojcimierza - weryfikacja PR #21 (Skytest1).
// =============================================================================
#include "core/Math.h"
#include "scene/Scene.h"
#include <gtest/gtest.h>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <limits>
#include <string>
#include <thread>
#include <vector>
using namespace wz;
namespace {
const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();
int CountLines(const std::string& s, const std::string& needle) {
  int n = 0; size_t p = 0; while ((p = s.find(needle, p)) != std::string::npos) { ++n; p += needle.size(); } return n; }
bool Unit(const glm::quat& q) { return std::abs(glm::length(q) - 1.0f) < 1e-5f; }
}

TEST(PR21, AllSettersKeepPreviousAndDoNotBumpVersion) {
  Scene s; auto& t = s.CreateGameObject().transform();
  t.SetLocalPosition({1,2,3}); t.SetLocalScale({2,2,2}); t.SetLocalEulerAngles({10,20,30});
  const auto p = t.GetLocalPosition(); const auto r = t.GetLocalRotation(); const auto sc = t.GetLocalScale();
  testing::internal::CaptureStderr();
  const auto v0 = t.GetChangeVersion();
  t.SetLocalPosition({kNaN,0,0}); t.SetPosition({kInf,0,0}); t.SetLocalScale({0,kNaN,0});
  t.SetLocalEulerAngles({kNaN,0,0}); t.Translate({kNaN,0,0}); t.Translate({0,kInf,0}, true);
  t.Rotate({0,kNaN,0}); t.Rotate({kInf,0,0}, true); t.LookAt({kNaN,0,0}); t.LookAt({0,0,-5},{kNaN,0,0});
  const auto vAfterVec = t.GetChangeVersion();
  t.SetLocalRotation(glm::quat(kNaN,0,0,0)); t.SetLocalRotation(glm::quat(0,0,0,0));
  t.SetRotation(glm::quat(0,0,0,0)); t.SetRotation(glm::quat(1,kInf,0,0));
  const auto vAfterQuat = t.GetChangeVersion();
  t.SetWorldPoseFromPhysics({kNaN,0,0}, glm::quat(0,0,0,0));
  (void)testing::internal::GetCapturedStderr();
  EXPECT_EQ(t.GetLocalPosition(), p); EXPECT_EQ(t.GetLocalScale(), sc);
  EXPECT_NEAR(std::abs(glm::dot(t.GetLocalRotation(), r)), 1.0f, 1e-6f);
  EXPECT_EQ(vAfterVec, v0) << "odrzucone wektory nie powinny podbijac wersji";
  EXPECT_EQ(vAfterQuat, v0) << "odrzucone kwaterniony podbijaja changeVersion";
}

TEST(PR21, LookAtParallelUpKeepsRotation) {
  Scene s; auto& t = s.CreateGameObject().transform();
  t.SetLocalEulerAngles({0,45,0}); const auto r = t.GetLocalRotation();
  testing::internal::CaptureStderr();
  t.LookAt({0,5,0}, {0,1,0}); t.LookAt({0,0,-5}, {0,0,0});
  (void)testing::internal::GetCapturedStderr();
  EXPECT_TRUE(IsFinite(t.GetLocalRotation()));
  EXPECT_TRUE(Unit(t.GetLocalRotation()));
  (void)r;
}

TEST(PR21, ZeroScaleParentDoesNotPoisonChild) {
  Scene s; auto& par = s.CreateGameObject().transform(); auto& ch = s.CreateGameObject().transform();
  par.SetLocalPosition({1,0,0}); ch.SetLocalPosition({0,2,0}); ch.SetParent(&par, false);
  par.SetLocalScale({0,0,0});
  testing::internal::CaptureStderr();
  ch.SetPosition({5,5,5}); ch.Translate({1,0,0}, true); ch.SetRotation(glm::quat(glm::vec3(0,1,0)));
  ch.LookAt({10,0,0}); ch.SetWorldPoseFromPhysics({3,3,3}, glm::quat(glm::vec3(0.5f,0,0)));
  (void)testing::internal::GetCapturedStderr();
  EXPECT_TRUE(IsFinite(ch.GetLocalPosition())); EXPECT_TRUE(IsFinite(ch.GetLocalRotation()));
  EXPECT_TRUE(IsFinite(ch.GetLocalScale())); EXPECT_TRUE(Unit(ch.GetLocalRotation()));
  EXPECT_TRUE(IsFinite(ch.GetPosition()));
  // reparent pod zerowa skale z worldPositionStays i z powrotem
  auto& other = s.CreateGameObject().transform(); other.SetLocalPosition({7,7,7});
  testing::internal::CaptureStderr();
  other.SetParent(&par, true); other.SetParent(nullptr, true);
  (void)testing::internal::GetCapturedStderr();
  EXPECT_TRUE(IsFinite(other.GetLocalPosition())); EXPECT_TRUE(IsFinite(other.GetLocalScale()));
  EXPECT_TRUE(IsFinite(other.GetLocalRotation()));
  // odrodzenie rodzica
  par.SetLocalScale({1,1,1}); EXPECT_EQ(ch.GetPosition(), glm::vec3(1,2,0)) << "pozycja dziecka sie zmienila";
}

TEST(PR21, WarnRateLimitFirstThenEvery1000) {
  const char* w = "PR21.rate";
  testing::internal::CaptureStderr();
  for (int i = 0; i < 2500; ++i) (void)SanitizeFloat(kNaN, 0.f, w);
  const std::string out = testing::internal::GetCapturedStderr();
  EXPECT_EQ(CountLines(out, "\n"), 3) << out;
  EXPECT_EQ(CountLines(out, "'PR21.rate'"), 3);
  EXPECT_NE(out.find("1000 razy"), std::string::npos); EXPECT_NE(out.find("2000 razy"), std::string::npos);
  EXPECT_EQ(NonFiniteWarningCount(w), 2500u);
  // inne 'what' liczy osobno; nullptr nie crashuje
  testing::internal::CaptureStderr();
  (void)SanitizeFloat(kInf, 0.f, "PR21.other"); (void)SanitizeFloat(kInf, 0.f, nullptr);
  const std::string o2 = testing::internal::GetCapturedStderr();
  EXPECT_EQ(CountLines(o2, "\n"), 2) << o2;
}

TEST(PR21, WarnEightThreads) {
  const char* w = "PR21.mt";
  testing::internal::CaptureStderr();
  std::vector<std::thread> th;
  for (int k = 0; k < 8; ++k) th.emplace_back([&]{ for (int i = 0; i < 12500; ++i) (void)SanitizeFloat(kNaN, 0.f, w); });
  for (auto& x : th) x.join();
  const std::string out = testing::internal::GetCapturedStderr();
  EXPECT_EQ(NonFiniteWarningCount(w), 100000u);
  EXPECT_EQ(CountLines(out, "'PR21.mt'"), 1 + 100) << "1 pelna + 100 podsumowan";
  EXPECT_EQ(CountLines(out, "\n"), 101);
}

TEST(PR21, SanitizeRotationNormalizedAndTiny) {
  const glm::quat fb(1,0,0,0);
  testing::internal::CaptureStderr();
  auto a = SanitizeRotation(glm::quat(2,0,0,0), fb, "PR21.q"); EXPECT_TRUE(Unit(a));
  auto b = SanitizeRotation(glm::quat(1e30f,1e30f,1e30f,1e30f), fb, "PR21.q"); EXPECT_TRUE(Unit(b)) << b.w;
  auto c = SanitizeRotation(glm::quat(3e38f,3e38f,0,0), fb, "PR21.q"); EXPECT_TRUE(Unit(c));
  auto d = SanitizeRotation(glm::quat(0.3f,0.1f,-0.5f,0.2f), fb, "PR21.q"); EXPECT_TRUE(Unit(d));
  // maly ale niezerowy: 1e-5 (len2=1e-10) przechodzi, 1e-7 (len2=1e-14) -> fallback
  auto e = SanitizeRotation(glm::quat(0, 1e-5f, 0, 0), fb, "PR21.q"); EXPECT_TRUE(Unit(e)); EXPECT_NEAR(e.x, 1.f, 1e-5f);
  const auto before = NonFiniteWarningCount("PR21.tiny");
  auto f = SanitizeRotation(glm::quat(0, 1e-7f, 0, 0), fb, "PR21.tiny");
  auto g = SanitizeRotation(glm::quat(0, 1e-40f, 0, 0), fb, "PR21.tiny");  // denormal
  auto h = SanitizeRotation(glm::quat(-0.f, 0, 0, 0), fb, "PR21.tiny");
  (void)testing::internal::GetCapturedStderr();
  EXPECT_EQ(f, fb); EXPECT_EQ(g, fb); EXPECT_EQ(h, fb);
  EXPECT_EQ(NonFiniteWarningCount("PR21.tiny") - before, 3u);
}

// ---- Runda 2 (#21 na 1bb1614) ----
namespace {
struct Probe { std::uint32_t v; bool dirtyBefore; };
void Clean(Transform& t) { (void)t.GetLocalToWorldMatrix(); }
}

TEST(PR21b, RejectedAndIdenticalDoNotTouchVersionOrDirty) {
  Scene s; auto& par = s.CreateGameObject().transform(); auto& t = s.CreateGameObject().transform();
  t.SetParent(&par, false);
  t.SetLocalPosition({1,2,3}); t.SetLocalScale({2,2,2}); t.SetLocalEulerAngles({10,20,30});
  Clean(t); Clean(par);
  const auto v0 = t.GetChangeVersion(); const auto pv0 = par.GetChangeVersion();
  ASSERT_FALSE(t.IsWorldMatrixDirty());
  testing::internal::CaptureStderr();
  t.SetLocalPosition({kNaN,0,0}); t.SetPosition({kInf,0,0}); t.SetLocalScale({0,kNaN,0});
  t.SetLocalEulerAngles({kNaN,0,0}); t.Translate({kNaN,0,0}); t.Translate({0,kInf,0}, true);
  t.Rotate({0,kNaN,0}); t.Rotate({kInf,0,0}, true); t.LookAt({kNaN,0,0}); t.LookAt({0,0,-5},{kNaN,0,0});
  t.LookAt({5,0,0},{0,0,0}); t.LookAt(t.GetPosition() + glm::vec3(0,3,0), {0,1,0});
  t.LookAt(t.GetPosition(), {0,1,0});
  t.SetLocalRotation(glm::quat(kNaN,0,0,0)); t.SetLocalRotation(glm::quat(0,0,0,0));
  t.SetRotation(glm::quat(0,0,0,0)); t.SetRotation(glm::quat(1,kInf,0,0));
  t.SetWorldPoseFromPhysics({kNaN,0,0}, glm::quat(0,0,0,0));
  t.SetParent(&par, true); t.SetParent(&t, true);
  (void)testing::internal::GetCapturedStderr();
  EXPECT_EQ(t.GetChangeVersion(), v0) << "odrzucone";
  EXPECT_FALSE(t.IsWorldMatrixDirty()) << "odrzucone -> dirty";
  // identyczne wartosci
  t.SetLocalPosition(t.GetLocalPosition()); t.SetLocalScale(t.GetLocalScale());
  t.SetLocalRotation(t.GetLocalRotation()); t.SetPosition(t.GetPosition());
  EXPECT_EQ(t.GetChangeVersion(), v0) << "identyczne Local*/SetPosition";
  EXPECT_FALSE(t.IsWorldMatrixDirty());
  auto step = [&](const char* name, auto fn) { Clean(t); const auto b = t.GetChangeVersion(); fn();
    std::printf("  [info] %-34s wersja +%d, dirty=%d\n", name, int(t.GetChangeVersion() - b), int(t.IsWorldMatrixDirty())); return t.GetChangeVersion() - b; };
  EXPECT_EQ(step("SetRotation(GetRotation()) z rodzicem", [&]{ t.SetRotation(t.GetRotation()); }), 0u);
  EXPECT_EQ(step("Translate(0) lokalnie", [&]{ t.Translate({0,0,0}); }), 0u);
  EXPECT_EQ(step("Translate(0) w swiecie", [&]{ t.Translate({0,0,0}, true); }), 0u);
  EXPECT_EQ(step("Rotate(0) lokalnie", [&]{ t.Rotate({0,0,0}); }), 0u);
  EXPECT_EQ(step("Rotate(0) w swiecie", [&]{ t.Rotate({0,0,0}, true); }), 0u);
  step("SetLocalEulerAngles(Get)", [&]{ t.SetLocalEulerAngles(t.GetLocalEulerAngles()); });
  t.SetParent(nullptr, true);
  EXPECT_EQ(step("SetRotation(GetRotation()) bez rodzica", [&]{ t.SetRotation(t.GetRotation()); }), 0u);
  EXPECT_EQ(step("Rotate(0) bez rodzica", [&]{ t.Rotate({0,0,0}); }), 0u);
  EXPECT_EQ(step("Translate(0) bez rodzica", [&]{ t.Translate({0,0,0}, true); }), 0u);
  t.SetParent(&par, true); Clean(t);
  t.SetWorldPoseFromPhysics(t.GetPosition(), t.GetRotation());
  EXPECT_FALSE(t.IsWorldMatrixDirty()) << "fizyka z ta sama poza";
  EXPECT_EQ(par.GetChangeVersion(), pv0);
}

TEST(PR21b, PhysicsPosePartialAccept) {
  Scene s; auto& t = s.CreateGameObject().transform();
  t.SetLocalPosition({1,1,1}); const auto r0 = t.GetLocalRotation(); const auto v0 = t.GetChangeVersion();
  testing::internal::CaptureStderr();
  const glm::quat good = glm::normalize(glm::quat(glm::vec3(0.3f,0.2f,0.1f)));
  t.SetWorldPoseFromPhysics({kNaN,0,0}, good);
  EXPECT_EQ(t.GetLocalPosition(), glm::vec3(1,1,1)); EXPECT_NEAR(std::abs(glm::dot(t.GetLocalRotation(), good)), 1.f, 1e-6f);
  t.SetWorldPoseFromPhysics({4,5,6}, glm::quat(kNaN,0,0,0));
  t.SetWorldPoseFromPhysics({7,8,9}, glm::quat(0,0,0,0));
  (void)testing::internal::GetCapturedStderr();
  EXPECT_EQ(t.GetLocalPosition(), glm::vec3(7,8,9)); EXPECT_NEAR(std::abs(glm::dot(t.GetLocalRotation(), good)), 1.f, 1e-6f);
  EXPECT_EQ(t.GetChangeVersion(), v0) << "fizyka nie podbija wersji";
  (void)r0;
  // z rodzicem o skali 0: pozycja odrzucona, obrot przyjety
  auto& par = s.CreateGameObject().transform(); t.SetParent(&par, false); par.SetLocalScale({0,0,0});
  const auto lp = t.GetLocalPosition();
  testing::internal::CaptureStderr();
  t.SetWorldPoseFromPhysics({1,2,3}, glm::quat(glm::vec3(0,1,0)));
  (void)testing::internal::GetCapturedStderr();
  EXPECT_EQ(t.GetLocalPosition(), lp); EXPECT_NEAR(std::abs(glm::dot(t.GetLocalRotation(), glm::quat(glm::vec3(0,1,0)))), 1.f, 1e-6f);
}

TEST(PR21b, LookAtUpEdgeCases) {
  Scene s; auto& t = s.CreateGameObject().transform();
  t.SetLocalEulerAngles({0,45,0}); const auto r0 = t.GetLocalRotation(); const auto v0 = t.GetChangeVersion();
  testing::internal::CaptureStderr();
  const auto c0 = NonFiniteWarningCount("Transform.LookAt (gora zerowa lub rownolegla do kierunku)");
  t.LookAt({0,5,0}, {0,1,0});      // rownolegle
  t.LookAt({0,-5,0}, {0,1,0});     // antyrownolegle
  t.LookAt({0,5,0}, {0,0,0});      // zero
  t.LookAt({0,5,0}, {0,1e-30f,0}); // malutkie, rownolegle
  t.LookAt({0,0,0}, {0,1,0});      // cel w tym samym punkcie
  const auto c1 = NonFiniteWarningCount("Transform.LookAt (gora zerowa lub rownolegla do kierunku)");
  (void)testing::internal::GetCapturedStderr();
  EXPECT_EQ(c1 - c0, 4u) << "4 odrzucenia up (cel w tym samym punkcie po cichu)";
  EXPECT_EQ(t.GetLocalRotation(), r0); EXPECT_EQ(t.GetChangeVersion(), v0);
  // prawie rownolegle: kat a do osi Y
  for (double a : {1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7}) {
    Scene s2; auto& u = s2.CreateGameObject().transform(); u.SetLocalEulerAngles({0,45,0});
    const auto before = u.GetLocalRotation();
    const glm::vec3 target(float(std::sin(a)) * 10.f, float(std::cos(a)) * 10.f, 0.f);
    testing::internal::CaptureStderr(); u.LookAt(target, {0,1,0}); const std::string e = testing::internal::GetCapturedStderr();
    const auto q = u.GetLocalRotation();
    const bool accepted = !(q == before);
    const glm::vec3 fwd = q * glm::vec3(0,0,-1); const glm::vec3 up = q * glm::vec3(0,1,0);
    const float err = glm::length(fwd - glm::normalize(target));
    std::printf("  [info] kat %.0e rad: %s, |q|=%.7f, blad kierunku=%.2e, up.y=%.4f\n", a, accepted ? "przyjety" : "odrzucony",
                glm::length(q), err, up.y);
    EXPECT_TRUE(IsFinite(q)); EXPECT_TRUE(Unit(q));
    if (accepted) EXPECT_LT(err, 1e-3f) << "kat " << a;
  }
}

// ---- Runda 3 (#21 na 161cf9e): zmiana w tolerancji ma byc ZAPISANA ----
TEST(PR21c, SmallStepsAccumulatePosition) {
  struct Case { const char* name; bool parent; bool translate; };
  for (Case c : {Case{"SetPosition bez rodzica", false, false}, Case{"SetPosition pod rodzicem", true, false},
                 Case{"Translate(world) bez rodzica", false, true}}) {
    Scene s; auto& par = s.CreateGameObject().transform(); auto& t = s.CreateGameObject().transform();
    if (c.parent) { par.SetLocalEulerAngles({10,20,30}); par.SetLocalScale({2,0.5f,3}); t.SetParent(&par, false); }
    t.SetPosition({1000,0,0}); const glm::vec3 p0 = t.GetPosition(); const auto v0 = t.GetChangeVersion();
    for (int i = 0; i < 1000; ++i) { if (c.translate) t.Translate({0.002f,0,0}, true); else t.SetPosition(t.GetPosition() + glm::vec3(0.002f,0,0)); }
    const float moved = t.GetPosition().x - p0.x;
    std::printf("  [info] %-28s 1000 x 2 mm przy |p|=1000: przesuniecie %.4f m, wersja +%u\n", c.name, moved, t.GetChangeVersion() - v0);
    EXPECT_NEAR(moved, 2.0f, 0.05f) << c.name;
  }
}
TEST(PR21c, SmallStepsAccumulateRotation) {
  const float step = 1e-7f;  // rad
  struct Case { const char* name; int mode; bool parent; };
  for (Case c : {Case{"Rotate lokalny", 0, false}, Case{"Rotate world", 1, false}, Case{"SetRotation(q*Get)", 2, false},
                 Case{"SetRotation pod rodzicem", 2, true}, Case{"SetLocalRotation(Get*q)", 3, false}}) {
    Scene s; auto& par = s.CreateGameObject().transform(); auto& t = s.CreateGameObject().transform();
    if (c.parent) { par.SetLocalEulerAngles({10,20,30}); par.SetLocalScale({2,0.5f,3}); t.SetParent(&par, false); }
    const glm::quat r0 = t.GetRotation(); const glm::quat dq = glm::angleAxis(step, glm::vec3(0,1,0));
    for (int i = 0; i < 10000; ++i) {
      switch (c.mode) { case 0: t.Rotate({0, glm::degrees(step), 0}); break; case 1: t.Rotate({0, glm::degrees(step), 0}, true); break;
        case 2: t.SetRotation(dq * t.GetRotation()); break; default: t.SetLocalRotation(t.GetLocalRotation() * dq); }
    }
    const float ang = 2.f * std::acos(std::min(1.f, std::abs(glm::dot(glm::normalize(r0), glm::normalize(t.GetRotation())))));
    std::printf("  [info] %-26s 10000 x 1e-7 rad: kat %.3e rad\n", c.name, ang);
    // Pod rodzicem 1e-7 rad jest ponizej rozdzielczosci floata po przeliczeniu przez rodzica
    // (szum round-tripu do 1.5 eps na krok vs sygnal ~0.4 eps) - tylko informacyjnie.
    if (c.parent) EXPECT_GT(ang, 5e-4f) << c.name; else EXPECT_NEAR(ang, 1e-3f, 5e-5f) << c.name;
  }
}
TEST(PR21c, Issue27AndMicroRotation) {
  Scene s; auto& g = s.CreateGameObject().transform(); auto& par = s.CreateGameObject().transform(); auto& t = s.CreateGameObject().transform();
  g.SetLocalEulerAngles({-40,5,70}); g.SetLocalScale({0.7f,1.6f,1.1f}); par.SetParent(&g,false);
  par.SetLocalEulerAngles({10,20,30}); par.SetLocalScale({2,0.5f,3}); t.SetParent(&par, false);
  t.SetLocalEulerAngles({33,-71,12}); t.SetLocalPosition({3,-4,5}); (void)t.GetLocalToWorldMatrix();
  const auto v0 = t.GetChangeVersion();
  t.SetRotation(t.GetRotation()); t.SetPosition(t.GetPosition()); t.SetRotation(-t.GetRotation());
  EXPECT_EQ(t.GetChangeVersion(), v0) << "#27 / SetPosition(Get) pod rodzicem / -q";
  EXPECT_FALSE(t.IsWorldMatrixDirty());
  t.SetRotation(glm::angleAxis(1e-6f, glm::normalize(glm::vec3(0.3f,-0.8f,0.5f))) * t.GetRotation());
  EXPECT_EQ(t.GetChangeVersion(), v0 + 1) << "1e-6 rad w swiecie";
  t.Rotate({glm::degrees(1e-6f),0,0}); EXPECT_EQ(t.GetChangeVersion(), v0 + 2) << "1e-6 rad Rotate lokalny";
}
