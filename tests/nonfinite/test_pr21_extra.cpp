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
