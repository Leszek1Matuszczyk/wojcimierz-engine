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

} // namespace
