// ============================================================
//  Wojcimierz Engine: tests/nonfinite/test_nonfinite_physics.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Rigidbody i collidery: kazda metoda i kazde pole REFLECT dostaje NaN / +-Inf.
// Wymaganie: po 10 krokach fizyki ofiara i niezalezne cialo obok maja skonczone
// pozy i predkosci, a "swiadek" spada normalnie (trucizna nie przechodzi przez Jolta).
#include "NonFiniteUtils.h"
#include "physics/Collider.h"
#include "physics/PhysicsWorld.h"
#include "physics/Rigidbody.h"
#include "scene/Scene.h"

#include <functional>
#include <string>

using namespace wz;
using namespace wztest;

namespace {

constexpr float kDt = 1.0f / 60.0f;

GameObject& Ball(Scene& s, const glm::vec3& p) {
    auto& b = s.CreateGameObject("Ball");
    b.transform().SetPosition(p);
    b.AddComponent<SphereCollider>().radius = 0.5f;
    b.AddComponent<Rigidbody>();
    return b;
}

using Poison = std::function<void(GameObject& victim, float bad, int axis)>;

void RunPoisoned(const Poison& poison, float bad, int axis, bool beforeFirstStep) {
    Scene s;
    PhysicsWorld w(s);
    auto& victim = Ball(s, {0, 10, 0});
    auto& witness = Ball(s, {5, 10, 0});
    if (!beforeFirstStep) w.FixedUpdate(kDt);
    poison(victim, bad, axis);
    for (int i = 0; i < 10; ++i) w.FixedUpdate(kDt);

    EXPECT_TRUE(Finite(victim.transform().GetPosition())) << "pozycja ofiary";
    EXPECT_TRUE(Finite(victim.transform().GetRotation())) << "obrot ofiary";
    EXPECT_TRUE(Finite(victim.GetComponent<Rigidbody>()->GetVelocity())) << "predkosc ofiary";
    EXPECT_TRUE(Finite(victim.GetComponent<Rigidbody>()->GetAngularVelocity())) << "predkosc katowa ofiary";
    const glm::vec3 wp = witness.transform().GetPosition();
    EXPECT_TRUE(Finite(wp)) << "trucizna przeszla na inne cialo";
    EXPECT_LT(wp.y, 10.0f) << "swiadek przestal spadac";
    EXPECT_NEAR(wp.x, 5.0f, 1e-3f);
}

struct Setter {
    const char* name;
    Poison poison;
};

const Setter kSetters[] = {
    {"SetVelocity", [](GameObject& g, float b, int a) { g.GetComponent<Rigidbody>()->SetVelocity(PoisonVec(b, a)); }},
    {"SetAngularVelocity",
     [](GameObject& g, float b, int a) { g.GetComponent<Rigidbody>()->SetAngularVelocity(PoisonVec(b, a)); }},
    {"AddForce", [](GameObject& g, float b, int a) { g.GetComponent<Rigidbody>()->AddForce(PoisonVec(b, a)); }},
    {"AddForceImpulse",
     [](GameObject& g, float b, int a) { g.GetComponent<Rigidbody>()->AddForce(PoisonVec(b, a), ForceMode::Impulse); }},
    {"AddTorque", [](GameObject& g, float b, int a) { g.GetComponent<Rigidbody>()->AddTorque(PoisonVec(b, a)); }},
    {"MovePosition", [](GameObject& g, float b, int a) { g.GetComponent<Rigidbody>()->MovePosition(PoisonVec(b, a)); }},
    {"MoveRotation", [](GameObject& g, float b, int a) { g.GetComponent<Rigidbody>()->MoveRotation(PoisonQuat(b, a)); }},
    {"mass", [](GameObject& g, float b, int) { g.GetComponent<Rigidbody>()->mass = b; }},
    {"linearDamping", [](GameObject& g, float b, int) { g.GetComponent<Rigidbody>()->linearDamping = b; }},
    {"angularDamping", [](GameObject& g, float b, int) { g.GetComponent<Rigidbody>()->angularDamping = b; }},
    {"SphereCollider.radius", [](GameObject& g, float b, int) { g.GetComponent<SphereCollider>()->radius = b; }},
    {"TransformSetPosition", [](GameObject& g, float b, int a) { g.transform().SetPosition(PoisonVec(b, a)); }},
};

TEST(PhysicsNonFinite, EverySetterBeforeBodyExists) {
    for (const auto& st : kSetters)
        for (float bad : kBadValues)
            for (int axis = 0; axis < 3; ++axis) {
                SCOPED_TRACE(std::string(st.name) + " " + BadName(bad) + " os " + std::to_string(axis));
                RunPoisoned(st.poison, bad, axis, true);
            }
}

TEST(PhysicsNonFinite, EverySetterOnLiveBody) {
    for (const auto& st : kSetters)
        for (float bad : kBadValues)
            for (int axis = 0; axis < 3; ++axis) {
                SCOPED_TRACE(std::string(st.name) + " " + BadName(bad) + " os " + std::to_string(axis));
                RunPoisoned(st.poison, bad, axis, false);
            }
}

TEST(PhysicsNonFinite, BoxColliderSize) {
    for (float bad : kBadValues)
        for (int axis = 0; axis < 3; ++axis) {
            SCOPED_TRACE(BadName(bad) + " os " + std::to_string(axis));
            Scene s;
            PhysicsWorld w(s);
            auto& box = s.CreateGameObject("Box");
            box.transform().SetPosition({0, 10, 0});
            box.AddComponent<BoxCollider>().size = PoisonVec(bad, axis, {1, 1, 1});
            box.AddComponent<Rigidbody>();
            auto& witness = Ball(s, {5, 10, 0});
            for (int i = 0; i < 10; ++i) w.FixedUpdate(kDt);
            EXPECT_TRUE(Finite(box.transform().GetPosition()));
            EXPECT_TRUE(Finite(witness.transform().GetPosition()));
        }
}

TEST(PhysicsNonFinite, SetGravity) {
    for (float bad : kBadValues)
        for (int axis = 0; axis < 3; ++axis) {
            SCOPED_TRACE(BadName(bad) + " os " + std::to_string(axis));
            Scene s;
            PhysicsWorld w(s);
            auto& b = Ball(s, {0, 10, 0});
            w.SetGravity(PoisonVec(bad, axis, {0, -9.81f, 0}));
            EXPECT_TRUE(Finite(w.GetGravity()));
            for (int i = 0; i < 10; ++i) w.FixedUpdate(kDt);
            EXPECT_TRUE(Finite(b.transform().GetPosition()));
        }
}

// Skonczone, ale ogromne wymiary: IsFinite ich nie lapie, a ksztalt (albo iloczyn
// rozmiar * skala) przepelnia sie w Jolcie do Inf/NaN i truje takze sasiednie ciala.
TEST(PhysicsNonFinite, HugeFiniteShapesDoNotPoisonWorld) {
    struct Huge {
        const char* name;
        void (*apply)(GameObject&);
    };
    const Huge cases[] = {
        {"radius=FLT_MAX", [](GameObject& g) { g.GetComponent<SphereCollider>()->radius = 3.4e38f; }},
        {"radius=1e30*scale=1e10",
         [](GameObject& g) {
             g.GetComponent<SphereCollider>()->radius = 1e30f;
             g.transform().SetLocalScale(glm::vec3(1e10f));
         }},
        {"BoxCollider.size=FLT_MAX", [](GameObject& g) { g.AddComponent<BoxCollider>().size = glm::vec3(3.4e38f); }},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        Scene s;
        PhysicsWorld w(s);
        auto& victim = Ball(s, {0, 10, 0});
        auto& witness = Ball(s, {5, 10, 0});
        c.apply(victim);
        for (int i = 0; i < 30; ++i) w.FixedUpdate(kDt);
        EXPECT_TRUE(Finite(victim.transform().GetPosition())) << "pozycja ofiary";
        EXPECT_TRUE(Finite(witness.transform().GetPosition())) << "trucizna przeszla na inne cialo";
    }
}

// Skala 0, mikro i ujemna: ksztalt dostaje dodatnie minimum (abs dla ujemnej), cialo
// zostaje skonczone, a swiadek obok lezy spokojnie na ziemi.
template <class C>
void DegenerateScaleCase(const glm::vec3& scale) {
    Scene s;
    PhysicsWorld w(s);
    auto& ground = s.CreateGameObject("Ground");
    ground.transform().SetPosition({0, -0.5f, 0});
    ground.AddComponent<BoxCollider>().size = {20, 1, 20};
    GameObject& v = s.CreateGameObject("V");
    v.transform().SetPosition({0, 3, 0});
    v.AddComponent<C>();
    v.AddComponent<Rigidbody>();
    v.transform().SetLocalScale(scale);
    auto& wit = Ball(s, {3, 3, 0});
    for (int i = 0; i < 120; ++i) w.FixedUpdate(kDt);
    EXPECT_TRUE(Finite(v.transform().GetPosition()));
    EXPECT_TRUE(Finite(v.GetComponent<Rigidbody>()->GetVelocity()));
    EXPECT_GT(v.transform().GetPosition().y, -0.5f) << "cialo przelecialo przez podloge";
    EXPECT_NEAR(wit.transform().GetPosition().y, 0.5f, 0.05f) << "swiadek nie lezy na ziemi";
}

TEST(PhysicsNonFinite, ZeroTinyAndNegativeScale) {
    const glm::vec3 scales[] = {glm::vec3(0), {0, 1, 1}, {1, 0, 1}, glm::vec3(1e-9f), glm::vec3(-1), {-2, 1, 1}};
    for (const auto& sc : scales) {
        SCOPED_TRACE(::testing::Message() << "skala (" << sc.x << "," << sc.y << "," << sc.z << ")");
        DegenerateScaleCase<SphereCollider>(sc);
        DegenerateScaleCase<BoxCollider>(sc);
        DegenerateScaleCase<CapsuleCollider>(sc);
    }
}

TEST(PhysicsNonFinite, NegativeScaleKeepsAbsoluteSize) {
    SphereCollider sph;
    BoxCollider box;
    EXPECT_FLOAT_EQ(sph.BuildShape(glm::vec3(-1)).radius, 0.5f);
    const auto b = box.BuildShape(glm::vec3(-2, 1, -1));
    EXPECT_FLOAT_EQ(b.halfExtents.x, 1.0f);
    EXPECT_FLOAT_EQ(b.halfExtents.y, 0.5f);
    EXPECT_FLOAT_EQ(b.halfExtents.z, 0.5f);
    const auto z = box.BuildShape(glm::vec3(0));
    EXPECT_GT(z.halfExtents.x, 0.0f);
}

// Zywe cialo: powiekszenie skali do 1e10 (przyciecie), potem powrot do 1 - ksztalt
// musi wrocic do 0.5 m, a po drodze nic nie staje sie NaN.
TEST(PhysicsNonFinite, LiveBodyScaleUpIsClampedAndReversible) {
    Scene s;
    PhysicsWorld w(s);
    auto& ground = s.CreateGameObject("Ground");
    ground.transform().SetPosition({0, -0.5f, 0});
    ground.AddComponent<BoxCollider>().size = {20, 1, 20};
    auto& v = Ball(s, {0, 3, 0});
    auto& wit = Ball(s, {30, 3, 0});
    for (int i = 0; i < 120; ++i) w.FixedUpdate(kDt);
    v.transform().SetLocalScale(glm::vec3(1e10f));
    for (int i = 0; i < 30; ++i) w.FixedUpdate(kDt);
    EXPECT_TRUE(Finite(v.transform().GetPosition())) << "ofiara po powiekszeniu";
    EXPECT_TRUE(Finite(wit.transform().GetPosition())) << "swiadek po powiekszeniu";
    v.transform().SetLocalScale(glm::vec3(1));
    v.transform().SetPosition({0, 3, 0});
    v.GetComponent<Rigidbody>()->SetVelocity({0, 0, 0});
    for (int i = 0; i < 180; ++i) w.FixedUpdate(kDt);
    EXPECT_NEAR(v.transform().GetPosition().y, 0.5f, 0.05f) << "po powrocie skali ksztalt nie wrocil";
}

// Ostrzezenie o przycieciu: dokladnie jedno na collider, mimo wielu przebudow i pol.
TEST(PhysicsNonFinite, ClampWarningOncePerCollider) {
    auto count = [](const std::string& log, const std::string& needle) {
        int n = 0;
        for (auto i = log.find(needle); i != std::string::npos; i = log.find(needle, i + 1)) ++n;
        return n;
    };
    ::testing::internal::CaptureStderr();
    {
        Scene s;
        PhysicsWorld w(s);
        auto& a = s.CreateGameObject("Alfa");
        a.transform().SetPosition({0, 10, 0});
        a.AddComponent<SphereCollider>().radius = 1e30f;
        a.AddComponent<Rigidbody>();
        auto& b = s.CreateGameObject("Beta");
        b.transform().SetPosition({50, 10, 0});
        auto& bc = b.AddComponent<BoxCollider>();
        bc.size = glm::vec3(1e30f);
        bc.center = glm::vec3(1e30f);
        b.AddComponent<Rigidbody>();
        for (int k = 0; k < 5; ++k) { // 5 przebudow ksztaltu przez zmiane skali
            a.transform().SetLocalScale(glm::vec3(1.0f + k));
            b.transform().SetLocalScale(glm::vec3(1.0f + k));
            for (int i = 0; i < 3; ++i) w.FixedUpdate(kDt);
        }
    }
    const std::string log = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(count(log, "'Alfa'"), 1) << log;
    EXPECT_EQ(count(log, "'Beta'"), 1) << log;
}

} // namespace
