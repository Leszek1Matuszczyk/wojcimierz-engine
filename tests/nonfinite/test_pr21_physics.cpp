// ============================================================
//  Wojcimierz Engine: tests/nonfinite/test_pr21_physics.cpp
//  Na chwałę czcigodnego Wojcimierza - #21 + fizyka: NaN nie teleportuje (Skytest1).
// ============================================================
#include "physics/PhysicsTestUtils.h"
#include "physics/IPhysicsBackend.h"
#include <limits>
using namespace wz; using namespace wztest;
namespace {
struct Counting : IPhysicsBackend {
  std::unique_ptr<IPhysicsBackend> in = CreateJoltBackend(); int setPose = 0, moveKin = 0;
  BodyHandle CreateBody(const BodyDesc& d) override { return in->CreateBody(d); }
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
  std::vector<ContactEvent> DrainContactEvents() override { return in->DrainContactEvents(); }
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
