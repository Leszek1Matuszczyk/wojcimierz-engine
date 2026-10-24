// ============================================================
//  Wojcimierz Engine: tests/physics/test_pr29.cpp
//  Na chwałę czcigodnego Wojcimierza - weryfikacja poprawki #29 w #18 (Skytest1).
// ============================================================
#include "PhysicsTestUtils.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <cstdlib>
using namespace wz; using namespace wztest;
namespace {
GameObject& Box(Scene& s, glm::vec3 p, glm::vec3 size, bool dynamic, glm::quat r = glm::quat(1, 0, 0, 0)) {
  auto& g = s.CreateGameObject("Box"); g.transform().SetPosition(p); g.transform().SetRotation(r);
  g.AddComponent<BoxCollider>().size = size; if (dynamic) g.AddComponent<Rigidbody>(); return g;
}
double Ang(glm::quat a, glm::quat b) { glm::dquat x(a), y(b); return 2 * std::acos(std::min(1.0, std::abs(glm::dot(glm::normalize(x), glm::normalize(y))))); }
}  // namespace

// Sen przy LookAt co klatke: kiedy zasypia, ile razy sie budzi, 600 klatek, 6 pozycji startowych.
TEST(PR29, LookAtSleepTimeline) {
  for (float x : {0.0f, 3.7f, -2.2f, 1.1f, 6.0f, -7.3f}) {
    Scene s; PhysicsWorld w(s); MakeGround(s); auto& ball = MakeBall(s, {x, 0.5f, 1.3f}); auto* rb = ball.GetComponent<Rigidbody>();
    Simulate(w, 120);
    int firstSleep = -1, wakes = 0, sleeping = 0; bool prev = rb->IsSleeping(); const glm::vec3 p0 = ball.transform().GetPosition();
    for (int f = 0; f < 600; ++f) {
      ball.transform().LookAt({5, 3, -4}); w.FixedUpdate(kDt);
      const bool sl = rb->IsSleeping(); sleeping += sl;
      if (sl && firstSleep < 0) firstSleep = f; if (prev && !sl) ++wakes; prev = sl;
    }
    std::printf("[PR29] LookAt x=%.1f: spal na starcie %d, pierwsze zasniecie w klatce %d, przebudzen %d, spi %d/600, przesuniecie %.2e m\n",
                x, rb->IsSleeping() ? 1 : 0, firstSleep, wakes, sleeping, glm::length(ball.transform().GetPosition() - p0));
  }
}

// Realny, ale maly ruch z gameplayu na AKTYWNYM ciele (spada z 50 m): 0.5 mm/klatke i 5e-4 rad/klatke.
TEST(PR29, SmallRealMotionOnActiveBodyIsApplied) {
  for (float x0 : {0.0f, 1000.0f}) for (int kind = 0; kind < 3; ++kind) {
    Scene s; PhysicsWorld w(s); auto& b = Box(s, {x0, 50, 0}, {1, 1, 1}, true);
    Simulate(w, 2);
    const float step = x0 > 0 ? 0.005f : 0.0005f; const float dAng = 5e-4f;
    const glm::quat q0 = b.transform().GetRotation(); const float xs = b.transform().GetPosition().x; int frames = 60;
    for (int f = 0; f < frames; ++f) {
      auto& t = b.transform();
      if (kind == 0) { glm::vec3 p = t.GetPosition(); p.x += step; t.SetPosition(p); }
      if (kind == 1) t.SetRotation(glm::angleAxis(dAng, glm::vec3(0, 1, 0)) * t.GetRotation());
      if (kind == 2) t.Translate({step, 0, 0}, true);
      w.FixedUpdate(kDt);
    }
    const float dx = b.transform().GetPosition().x - xs; const double ang = Ang(b.transform().GetRotation(), q0);
    const char* nm[] = {"SetPosition +x", "SetRotation +yaw", "Translate +x"};
    std::printf("[PR29] aktywne cialo |x0|=%.0f, %s: dx %.5f m (oczek. %.5f), kat %.5f rad (oczek. %.5f)\n", x0, nm[kind], dx,
                kind == 1 ? 0.0f : step * frames, ang, kind == 1 ? dAng * frames : 0.0f);
    if (kind == 1) EXPECT_NEAR(ang, dAng * frames, 0.1 * dAng * frames); else EXPECT_NEAR(dx, step * frames, 0.1f * step * frames);
  }
}

// Spiace pudelko podnoszone przez gameplay o 0.5 mm/klatke przez 200 klatek (10 cm): ma spasc, nie wisiec.
TEST(PR29, SleepingBodyLiftedInTinyStepsFallsBack) {
  for (float x0 : {0.0f, 1000.0f}) {
    Scene s; PhysicsWorld w(s); auto& g = MakeGround(s); g.transform().SetPosition({x0, -0.5f, 0});
    auto& b = Box(s, {x0, 0.5f, 0}, {1, 1, 1}, true); auto* rb = b.GetComponent<Rigidbody>();
    Simulate(w, 180); const bool asleep0 = rb->IsSleeping(); const float y0 = b.transform().GetPosition().y;
    const float step = x0 > 0 ? 0.005f : 0.0005f;
    for (int f = 0; f < 200; ++f) { glm::vec3 p = b.transform().GetPosition(); p.y += step; b.transform().SetPosition(p); w.FixedUpdate(kDt); }
    const float yLifted = b.transform().GetPosition().y; const bool asleepLifted = rb->IsSleeping();
    Simulate(w, 120);
    std::printf("[PR29] spiace pudelko x0=%.0f, +%.1f mm/klatke x200: spalo %d, po podnoszeniu y=%.4f (start %.4f) spi %d, 2 s pozniej y=%.4f spi %d\n",
                x0, step * 1000, asleep0, yLifted, y0, asleepLifted, b.transform().GetPosition().y, rb->IsSleeping());
    EXPECT_NEAR(b.transform().GetPosition().y, y0, 0.03f) << "pudelko wisi w powietrzu";
  }
}

// Wieza 5 i 10 pudelek, 10 s: dryf gornego, max |v| w ostatniej sekundzie, czy spia.
TEST(PR29, BoxTowerStability) {
  for (int n : {5, 10}) {
    Scene s; PhysicsWorld w(s); MakeGround(s); std::vector<GameObject*> bx;
    for (int i = 0; i < n; ++i) bx.push_back(&Box(s, {0, 0.5f + i * 1.0f, 0}, {1, 1, 1}, true));
    const glm::vec3 top0 = bx.back()->transform().GetPosition(); float vLast = 0; int sleepAll = -1;
    for (int f = 0; f < 600; ++f) {
      w.FixedUpdate(kDt);
      bool all = true; for (auto* b : bx) all &= b->GetComponent<Rigidbody>()->IsSleeping();
      if (all && sleepAll < 0) sleepAll = f;
      if (f >= 540) for (auto* b : bx) vLast = std::max(vLast, glm::length(b->GetComponent<Rigidbody>()->GetVelocity()));
    }
    const glm::vec3 d = bx.back()->transform().GetPosition() - top0; float tilt = 0;
    for (auto* b : bx) tilt = std::max(tilt, float(Ang(b->transform().GetRotation(), glm::quat(1, 0, 0, 0))));
    std::printf("[PR29] wieza %d: dryf gory (%.2e, %.2e, %.2e) m, max|v| w ost. 1 s %.2e, max przechyl %.2e rad, wszystkie spia od klatki %d\n",
                n, d.x, d.y, d.z, vLast, tilt, sleepAll);
    EXPECT_LT(std::hypot(d.x, d.z), 2e-2f); EXPECT_GT(d.y, -0.2f);
  }
}

// Pudelko na pochylni 25 st. (tan < 0.6, ma stac) i 35 st. (ma zjechac), 10 s.
TEST(PR29, BoxOnRamp) {
  for (float deg : {25.0f, 35.0f}) {
    Scene s; PhysicsWorld w(s); const glm::quat r = glm::angleAxis(glm::radians(deg), glm::vec3(0, 0, 1));
    Box(s, {0, 0, 0}, {20, 1, 4}, false, r);
    const glm::vec3 up = r * glm::vec3(0, 1, 0); auto& b = Box(s, up * 1.0f, {1, 1, 1}, true, r);
    Simulate(w, 30); const glm::vec3 p0 = b.transform().GetPosition(); float vmax = 0; int sleep = -1;
    for (int f = 0; f < 600; ++f) {
      w.FixedUpdate(kDt); vmax = std::max(vmax, glm::length(b.GetComponent<Rigidbody>()->GetVelocity()));
      if (sleep < 0 && b.GetComponent<Rigidbody>()->IsSleeping()) sleep = f;
    }
    const float slid = glm::length(b.transform().GetPosition() - p0);
    std::printf("[PR29] pochylnia %.0f st.: przesuniecie %.4f m w 10 s, max|v| %.3g, zasnelo w klatce %d\n", deg, slid, vmax, sleep);
    if (deg < 30) EXPECT_LT(slid, 5e-3f); else EXPECT_GT(slid, 1.0f);
  }
}

// Czas kroku: N pudelek i kul spadajacych na stos, 600 krokow, sredni i max czas kroku.
TEST(PR29, StepCost) {
  for (int n : {200, 500}) {
    Scene s; PhysicsWorld w(s); MakeGround(s); std::mt19937 rng(1); std::uniform_real_distribution<float> u(-4, 4);
    for (int i = 0; i < n; ++i) { if (i % 2) Box(s, {u(rng), 1 + i * 0.05f, u(rng)}, {0.6f, 0.6f, 0.6f}, true); else MakeBall(s, {u(rng), 1 + i * 0.05f, u(rng)}, 0.3f); }
    w.FixedUpdate(kDt);
    double total = 0, worst = 0; const int steps = 600;
    for (int f = 0; f < steps; ++f) {
      auto t0 = std::chrono::steady_clock::now(); w.FixedUpdate(kDt);
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); total += ms; worst = std::max(worst, ms);
    }
    std::printf("[PR29] koszt kroku, %d cial: sredni %.3f ms, max %.3f ms\n", n, total / steps, worst);
  }
}

// AwakeBallOnFlatGroundDoesNotDrift z losowymi startami: 50 kul (pozycja, obrot, promien), seed z WZ_TEST_SEED.
TEST(PR29, AwakeBallDriftRandomized) {
  unsigned seed = 0x5EED; if (const char* e = std::getenv("WZ_TEST_SEED")) seed = unsigned(std::strtoul(e, nullptr, 0));
  std::mt19937 rng(seed); std::uniform_real_distribution<float> u(-1, 1);
  float worstV = 0, worstD = 0; int over = 0;
  for (int i = 0; i < 50; ++i) {
    const float r = 0.2f + 0.4f * (u(rng) + 1); const glm::vec3 p{u(rng) * 8, r, u(rng) * 8};
    Scene s; PhysicsWorld w(s); MakeGround(s); auto& b = MakeBall(s, p, r);
    b.transform().SetRotation(glm::normalize(glm::quat(u(rng), u(rng), u(rng), u(rng))));
    auto* rb = b.GetComponent<Rigidbody>(); Simulate(w, 120); const glm::vec3 p0 = b.transform().GetPosition(); float vmax = 0;
    for (int f = 0; f < 600; ++f) { rb->WakeUp(); w.FixedUpdate(kDt); vmax = std::max(vmax, glm::length(rb->GetVelocity())); }
    const float d = glm::length(b.transform().GetPosition() - p0); worstV = std::max(worstV, vmax); worstD = std::max(worstD, d);
    over += vmax > 1e-4f;
  }
  std::printf("[PR29] dryf losowy seed=0x%x: 50 kul, max|v| %.3g m/s, max przesuniecie %.3g m / 10 s, kul z |v|>1e-4: %d\n", seed, worstV, worstD, over);
  EXPECT_LT(worstV, 1e-3f); EXPECT_LT(worstD, 1e-2f);
}
