// ============================================================
//  Wojcimierz Engine: tests/nonfinite/test_pr21_physics.cpp
//  Na chwałę czcigodnego Wojcimierza - #21 + fizyka: NaN nie teleportuje (Skytest1).
// ============================================================
#include "physics/PhysicsTestUtils.h"
#include "physics/IPhysicsBackend.h"
#include <limits>
#include <cstdio>
#include <string>
#include <cstring>
#include <cmath>
using namespace wz; using namespace wztest;
namespace {
struct Counting : IPhysicsBackend {
  std::unique_ptr<IPhysicsBackend> in = CreateJoltBackend(); int setPose = 0, moveKin = 0;
  std::vector<BodyHandle> hs;
  BodyHandle CreateBody(const BodyDesc& d) override { hs.push_back(in->CreateBody(d)); return hs.back(); }
  void DestroyBody(BodyHandle b) override { in->DestroyBody(b); }
  void SetPose(BodyHandle b, const glm::vec3& p, const glm::quat& r) override { ++setPose; in->SetPose(b, p, r); }
  void MoveKinematic(BodyHandle b, const glm::vec3& p, const glm::quat& r, float dt) override { ++moveKin; in->MoveKinematic(b, p, r, dt); }
  void GetPose(BodyHandle b, glm::vec3& p, glm::quat& r) const override { in->GetPose(b, p, r); }
  glm::vec3 GetLinearVelocity(BodyHandle b) const override { return in->GetLinearVelocity(b); }
  void SetLinearVelocity(BodyHandle b, const glm::vec3& v) override { in->SetLinearVelocity(b, v); }
  glm::vec3 GetAngularVelocity(BodyHandle b) const override { return in->GetAngularVelocity(b); }
  void SetAngularVelocity(BodyHandle b, const glm::vec3& w) override { in->SetAngularVelocity(b, w); }
  void AddForce(BodyHandle b, const glm::vec3& f) override { in->AddForce(b, f); }
  void AddImpulse(BodyHandle b, const glm::vec3& j) override { in->AddImpulse(b, j); }
  void AddTorque(BodyHandle b, const glm::vec3& t) override { in->AddTorque(b, t); }
  void AddAngularImpulse(BodyHandle b, const glm::vec3& j) override { in->AddAngularImpulse(b, j); }
  float GetMass(BodyHandle b) const override { return in->GetMass(b); }
  bool IsActive(BodyHandle b) const override { return in->IsActive(b); }
  void Activate(BodyHandle b) override { in->Activate(b); }
  void SetGravity(const glm::vec3& g) override { in->SetGravity(g); }
  glm::vec3 GetGravity() const override { return in->GetGravity(); }
  void Step(float dt) override { in->Step(dt); }
  int added = 0, removed = 0;
  std::vector<ContactEvent> DrainContactEvents() override { auto e = in->DrainContactEvents(); for (auto& x : e) (x.type == ContactEvent::Type::Added ? added : removed)++; return e; }
  std::optional<BackendRaycastHit> Raycast(const glm::vec3& o, const glm::vec3& d, float m, bool t) const override { return in->Raycast(o, d, m, t); }
  std::size_t GetBodyCount() const override { return in->GetBodyCount(); }
};
const float kNaN = std::numeric_limits<float>::quiet_NaN();
}
TEST(PR21Physics, NaNEveryFrameDoesNotTeleport) {
  Scene s; auto be = std::make_unique<Counting>(); Counting* c = be.get(); PhysicsWorld w(s, std::move(be));
  MakeGround(s); auto& ball = MakeBall(s, {0, 5, 0}); auto& kin = MakeBall(s, {3, 2, 0});
  kin.GetComponent<Rigidbody>()->bodyType = BodyType::Kinematic;
  Simulate(w, 2); const int sp0 = c->setPose, mk0 = c->moveKin;
  testing::internal::CaptureStderr();
  for (int i = 0; i < 120; ++i) {
    for (auto* go : {&ball, &kin}) {
      auto& t = go->transform();
      t.SetPosition({kNaN, 0, 0}); t.SetLocalRotation(glm::quat(kNaN, 0, 0, 0)); t.SetRotation(glm::quat(0, 0, 0, 0));
      t.LookAt(t.GetPosition() + glm::vec3(0, 1, 0)); t.SetLocalPosition(t.GetLocalPosition()); t.SetLocalScale(t.GetLocalScale());
    }
    w.FixedUpdate(kDt);
  }
  (void)testing::internal::GetCapturedStderr();
  EXPECT_EQ(c->setPose - sp0, 0); EXPECT_EQ(c->moveKin - mk0, 0);
  EXPECT_LT(ball.transform().GetPosition().y, 1.0f) << "kula spadla normalnie";
  // kontrola: prawdziwy ruch z gameplayu nadal teleportuje
  ball.transform().SetPosition({0, 4, 0}); w.FixedUpdate(kDt); EXPECT_EQ(c->setPose - sp0, 1);
}

// #21 f07e1d3: LookAt co klatke na nieruchomy cel, cialo dynamiczne, 240 klatek.
// Tryby: spoczynek (po rozgrzewkowym LookAt i ponownym zasnieciu) z LookAt i bez, lot z 20 m z LookAt i bez.
TEST(PR21Physics, LookAtEveryFrameOnDynamicBody) {
  const char* nm[] = {"spoczynek+LookAt", "spoczynek bez LookAt", "lot+LookAt", "lot bez LookAt"};
  float flightMinY[2] = {0, 0}, flightY[2] = {0, 0};
  for (int mode = 0; mode < 4; ++mode) {
    const bool rest = mode < 2, look = mode % 2 == 0;
    Scene s; auto be = std::make_unique<Counting>(); Counting* c = be.get(); PhysicsWorld w(s, std::move(be));
    MakeGround(s); auto& ball = MakeBall(s, {0, rest ? 0.5f : 20.0f, 0});
    const glm::vec3 target{5, 3, -4};
    if (rest) { Simulate(w, 120); ball.transform().LookAt(target); Simulate(w, 180); }
    const int sp0 = c->setPose, ad0 = c->added, rm0 = c->removed; int sleeping = 0, bumps = 0; float minY = 1e9f;
    std::string bumpFrames;
    for (int f = 0; f < 240; ++f) {
      auto& t = ball.transform(); const auto v0 = t.GetChangeVersion();
      if (look) t.LookAt(target);
      if (t.GetChangeVersion() != v0) { ++bumps; if (rest && bumpFrames.size() < 60) bumpFrames += std::to_string(f) + " "; }
      w.FixedUpdate(kDt);
      if (ball.GetComponent<Rigidbody>()->IsSleeping()) ++sleeping;
      minY = std::min(minY, ball.transform().GetPosition().y);
    }
    const glm::vec3 v = c->GetLinearVelocity(c->hs.back());
    std::printf("[PR21Physics] %s: podbicia %d/240 [%s], SetPose %d, spi %d/240, kontakty +%d -%d, min y %.4f, y %.4f, |v| %.4f\n", nm[mode], bumps,
                bumpFrames.c_str(), c->setPose - sp0, sleeping, c->added - ad0, c->removed - rm0, minY, ball.transform().GetPosition().y, glm::length(v));
    // Spoczynek+LookAt: podbicia NIE pochodza z Transformu (patrz SameQuatEveryFrameKeepsBodyAsleep), tylko z dryfu
    // obudzonej kuli w Jolt (DISABLED_AwakeBallOnFlatGroundDoesNotDrift): fizyka obraca kule, LookAt ja cofa.
    if (mode == 1) { EXPECT_EQ(bumps, 0); EXPECT_EQ(c->setPose - sp0, 0); EXPECT_EQ(sleeping, 240); }
    if (!rest) { flightMinY[mode - 2] = minY; flightY[mode - 2] = ball.transform().GetPosition().y; }
  }
  EXPECT_NEAR(flightMinY[0], flightMinY[1], 1e-3f) << "LookAt nie zmienia przebiegu zderzenia";
  EXPECT_NEAR(flightY[0], flightY[1], 1e-3f);
}

// Kinematyk ruszany SetPosition co klatke: predkosc klatka po klatce i dryf ciala.
// v=1 m/s przy x0=0 oraz 2 mm/klatke przy |p|=1000; z LookAt na staly cel i bez.
TEST(PR21Physics, KinematicVelocityFrameByFrame) {
  struct Cfg { const char* nm; glm::vec3 p0; float step; bool look; };
  for (Cfg cfg : {Cfg{"x0=0, 1/60 m", {0, 2, 0}, 1.0f / 60, false}, Cfg{"x0=0, 1/60 m + LookAt", {0, 2, 0}, 1.0f / 60, true},
                  Cfg{"|p|=1000, 2 mm", {1000, 2, 0}, 0.002f, false}, Cfg{"|p|=1000, 2 mm + LookAt", {1000, 2, 0}, 0.002f, true}}) {
    Scene s; auto be = std::make_unique<Counting>(); Counting* c = be.get(); PhysicsWorld w(s, std::move(be));
    auto& k = MakeBall(s, cfg.p0); k.GetComponent<Rigidbody>()->bodyType = BodyType::Kinematic;
    Simulate(w, 2);
    glm::vec3 p = k.transform().GetPosition(); const glm::vec3 pStart = p;
    const float realStep = (p.x + cfg.step) - p.x; const float vExp = realStep / kDt;  // float przy |x|=1000 zaokragla krok
    float vMin = 1e9f, vMax = -1e9f, drift = 0; int mk0 = c->moveKin, zeroV = 0;
    for (int f = 0; f < 300; ++f) {
      p.x += cfg.step; k.transform().SetPosition(p);
      if (cfg.look) k.transform().LookAt(cfg.p0 + glm::vec3(0, 0, -50));
      w.FixedUpdate(kDt);
      const float v = c->GetLinearVelocity(c->hs.back()).x;
      vMin = std::min(vMin, v); vMax = std::max(vMax, v); if (std::abs(v) < 1e-6f) ++zeroV;
      glm::vec3 bp; glm::quat br; c->GetPose(c->hs.back(), bp, br);
      drift = std::max(drift, glm::length(bp - p));
    }
    const glm::vec3 total = k.transform().GetPosition() - pStart; const float want = p.x - pStart.x;
    std::printf("[PR21Physics] kinematyk %s: MoveKinematic %d/300, v oczek. %.4f, min %.4f max %.4f, v=0 w %d klatkach, max dryf %.3g m, przejechal %.4f m (oczek. %.4f)\n",
                cfg.nm, c->moveKin - mk0, vExp, vMin, vMax, zeroV, drift, total.x, want);
    EXPECT_EQ(c->moveKin - mk0, 300); EXPECT_EQ(zeroV, 0);
    EXPECT_NEAR(vMin, vExp, 0.02f * vExp + 1e-3f); EXPECT_NEAR(vMax, vExp, 0.02f * vExp + 1e-3f);
    EXPECT_EQ(total.x, want); EXPECT_EQ(drift, 0.0f);
  }
}

// Kontrakt #21 w fizyce: ten sam kwaternion co klatke na spiacej kuli = 0 SetPose, kula spi.
TEST(PR21Physics, SameQuatEveryFrameKeepsBodyAsleep) {
  Scene s; auto be = std::make_unique<Counting>(); Counting* c = be.get(); PhysicsWorld w(s, std::move(be));
  MakeGround(s); auto& ball = MakeBall(s, {0, 0.5f, 0}); ball.transform().LookAt({5, 3, -4}); Simulate(w, 300);
  const glm::quat q = ball.transform().GetRotation(); const glm::vec3 p = ball.transform().GetPosition(); const int sp0 = c->setPose; int sleep = 0;
  for (int f = 0; f < 600; ++f) { ball.transform().SetRotation(q); ball.transform().SetPosition(p); w.FixedUpdate(kDt); sleep += ball.GetComponent<Rigidbody>()->IsSleeping(); }
  EXPECT_EQ(c->setPose - sp0, 0); EXPECT_EQ(sleep, 600);
}

// Fizyka (#17), nie #21: obudzona kula na plaskiej podlodze sama sie toczy i przyspiesza.
// Zmierzone: 10 s z Activate co klatke -> 0.15-0.2 m przesuniecia, |v| do 4 cm/s
// (poza symetrycznym x=0 bez obrotu). Wylaczone do czasu poprawki - zobacz issue.
TEST(PR21Physics, DISABLED_AwakeBallOnFlatGroundDoesNotDrift) {
  for (int rot = 0; rot < 2; ++rot) for (float x : {0.0f, 3.7f}) {
    Scene s; auto be = std::make_unique<Counting>(); Counting* c = be.get(); PhysicsWorld w(s, std::move(be));
    MakeGround(s); auto& ball = MakeBall(s, {x, 0.5f, 0}); if (rot) ball.transform().LookAt({5, 3, -4});
    Simulate(w, 120); auto h = c->hs.back(); const glm::vec3 p0 = ball.transform().GetPosition(); float vmax = 0;
    for (int f = 0; f < 600; ++f) { c->Activate(h); w.FixedUpdate(kDt); vmax = std::max(vmax, glm::length(c->GetLinearVelocity(h))); }
    const glm::vec3 d = ball.transform().GetPosition() - p0;
    std::printf("[PR21Physics] dryf rot=%d x=%.1f: max|v| %.3g m/s, przesuniecie (%.3g, %.3g, %.3g) m\n", rot, x, vmax, d.x, d.y, d.z);
    EXPECT_LT(glm::length(d), 1e-3f); EXPECT_LT(vmax, 1e-3f);
  }
}
