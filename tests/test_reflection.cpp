// ============================================================
//  Wojcimierz Engine: tests/test_reflection.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "scene/Reflection.h"
#include "scene/Scene.h"

#include <glm/gtc/quaternion.hpp>
#include <gtest/gtest.h>

namespace test_reflection {

enum class Mode { Idle = 0, Run = 3 };

class Sample : public wz::Component {
public:
    bool flag = false;
    std::int32_t count = 0;
    std::uint32_t mask = 0;
    float speed = 1.0f;
    std::string label = "x";
    glm::vec2 v2{0.0f};
    glm::vec3 v3{0.0f};
    glm::vec4 color{1.0f};
    glm::quat q{1.0f, 0.0f, 0.0f, 0.0f};
    wz::UUID target;
    Mode mode = Mode::Idle;

    float GetHealth() const { return health_; }
    void SetHealth(float h) { health_ = h < 0.0f ? 0.0f : h; }

private:
    float health_ = 100.0f;
};

// Komponent bez domyślnego konstruktora: można go opisać, ale nie utworzyć z pliku sceny.
class NoDefault : public wz::Component {
public:
    explicit NoDefault(int v) : value(v) {}
    std::int32_t value;
};

class Empty : public wz::Component {};

} // namespace test_reflection

// Kwalifikowana nazwa: w rejestrze trafi jako "Sample".
REFLECT(test_reflection::Sample,
        FIELD(flag), FIELD(count), FIELD(mask), FIELD(speed), FIELD(label),
        FIELD(v2), FIELD(v3), FIELD(color), FIELD(q), FIELD(target),
        FIELD_NAMED("behaviour", mode),
        PROPERTY("health", GetHealth, SetHealth))
REFLECT(test_reflection::NoDefault, FIELD(value))
REFLECT(test_reflection::Empty)

using namespace test_reflection;
using wz::ComponentRegistry;
using wz::FieldType;
using wz::FieldValue;

TEST(Reflection, TransformIsRegistered) {
    const auto* info = ComponentRegistry::Get().Find("Transform");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info, ComponentRegistry::Get().Find<wz::Transform>());
    ASSERT_EQ(info->fields.size(), 3u);
    EXPECT_EQ(info->fields[0].name, "position");
    EXPECT_EQ(info->fields[0].type, FieldType::Vec3);
    EXPECT_EQ(info->fields[1].name, "rotation");
    EXPECT_EQ(info->fields[1].type, FieldType::Quat);
    EXPECT_EQ(info->fields[2].name, "scale");
    EXPECT_EQ(info->fields[2].type, FieldType::Vec3);
}

TEST(Reflection, TransformPropertiesReadAndWrite) {
    wz::Scene scene;
    auto& go = scene.CreateGameObject();
    auto& t = go.transform();
    const auto* info = ComponentRegistry::Get().FindFor(t);
    ASSERT_NE(info, nullptr);

    const auto before = t.GetChangeVersion();
    ASSERT_TRUE(info->FindField("position")->set(t, FieldValue(glm::vec3(1, 2, 3))));
    ExpectVecNear(t.GetLocalPosition(), {1, 2, 3});
    EXPECT_GT(t.GetChangeVersion(), before) << "zapis przez refleksje musi isc przez setter";

    const glm::quat r = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    ASSERT_TRUE(info->FindField("rotation")->set(t, FieldValue(r)));
    const auto readBack = std::get<glm::quat>(info->FindField("rotation")->get(t));
    EXPECT_NEAR(glm::dot(readBack, r), 1.0f, 1e-5f);

    ASSERT_TRUE(info->FindField("scale")->set(t, FieldValue(glm::vec3(2))));
    ExpectVecNear(std::get<glm::vec3>(info->FindField("scale")->get(t)), {2, 2, 2});
}

TEST(Reflection, TransformAddToReturnsExisting) {
    wz::Scene scene;
    auto& go = scene.CreateGameObject();
    const auto* info = ComponentRegistry::Get().Find<wz::Transform>();
    ASSERT_TRUE(info->addTo);
    EXPECT_EQ(info->addTo(go), &go.transform());
    EXPECT_EQ(go.GetComponentCount(), 1u);
}

TEST(Reflection, AllFieldTypesAndQualifiedName) {
    const auto* info = ComponentRegistry::Get().Find("Sample");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info, ComponentRegistry::Get().Find<Sample>());

    const std::pair<const char*, FieldType> expected[] = {
        {"flag", FieldType::Bool},   {"count", FieldType::Int},  {"mask", FieldType::UInt},
        {"speed", FieldType::Float}, {"label", FieldType::String}, {"v2", FieldType::Vec2},
        {"v3", FieldType::Vec3},     {"color", FieldType::Vec4}, {"q", FieldType::Quat},
        {"target", FieldType::UUID}, {"behaviour", FieldType::Enum}, {"health", FieldType::Float},
    };
    ASSERT_EQ(info->fields.size(), std::size(expected));
    for (std::size_t i = 0; i < std::size(expected); ++i) {
        EXPECT_EQ(info->fields[i].name, expected[i].first);
        EXPECT_EQ(info->fields[i].type, expected[i].second) << expected[i].first;
    }
}

TEST(Reflection, FieldGetSetRoundTrip) {
    wz::Scene scene;
    auto& go = scene.CreateGameObject();
    auto& s = go.AddComponent<Sample>();
    const auto& info = *ComponentRegistry::Get().FindFor(s);

    const wz::UUID id = wz::UUID::Generate();
    EXPECT_TRUE(info.FindField("flag")->set(s, FieldValue(true)));
    EXPECT_TRUE(info.FindField("count")->set(s, FieldValue(std::int32_t{-7})));
    EXPECT_TRUE(info.FindField("mask")->set(s, FieldValue(std::uint32_t{0xF0u})));
    EXPECT_TRUE(info.FindField("speed")->set(s, FieldValue(2.5f)));
    EXPECT_TRUE(info.FindField("label")->set(s, FieldValue(std::string("hej"))));
    EXPECT_TRUE(info.FindField("target")->set(s, FieldValue(id)));
    EXPECT_TRUE(info.FindField("behaviour")->set(s, FieldValue(std::int32_t{3})));

    EXPECT_TRUE(s.flag);
    EXPECT_EQ(s.count, -7);
    EXPECT_EQ(s.mask, 0xF0u);
    EXPECT_FLOAT_EQ(s.speed, 2.5f);
    EXPECT_EQ(s.label, "hej");
    EXPECT_EQ(s.target, id);
    EXPECT_EQ(s.mode, Mode::Run);

    EXPECT_EQ(std::get<std::int32_t>(info.FindField("behaviour")->get(s)), 3);
    EXPECT_EQ(std::get<std::string>(info.FindField("label")->get(s)), "hej");
}

TEST(Reflection, PropertyGoesThroughSetter) {
    wz::Scene scene;
    auto& s = scene.CreateGameObject().AddComponent<Sample>();
    const auto* health = ComponentRegistry::Get().Find<Sample>()->FindField("health");
    ASSERT_TRUE(health->set(s, FieldValue(-5.0f)));
    EXPECT_FLOAT_EQ(s.GetHealth(), 0.0f); // setter przycina do zera
}

TEST(Reflection, WrongValueTypeIsRejected) {
    wz::Scene scene;
    auto& s = scene.CreateGameObject().AddComponent<Sample>();
    const auto* speed = ComponentRegistry::Get().Find<Sample>()->FindField("speed");
    EXPECT_FALSE(speed->set(s, FieldValue(std::string("szybko"))));
    EXPECT_FLOAT_EQ(s.speed, 1.0f);
}

TEST(Reflection, FactoryCreatesComponentByName) {
    wz::Scene scene;
    auto& go = scene.CreateGameObject();
    const auto* info = ComponentRegistry::Get().Find("Sample");
    ASSERT_TRUE(info->addTo);
    wz::Component* c = info->addTo(go);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(go.GetComponent<Sample>(), c);
    EXPECT_EQ(ComponentRegistry::Get().FindFor(*c), info);
}

namespace test_reflection {
class AwakeProbe : public wz::Component {
public:
    float value = 0.0f;
    float seenInAwake = -1.0f;
    void Awake() override { seenInAwake = value; }
};
} // namespace test_reflection
REFLECT(test_reflection::AwakeProbe, FIELD(value))

TEST(Reflection, CreateDefersAwakeUntilAttached) {
    wz::Scene scene;
    auto& go = scene.CreateGameObject();
    const auto* info = ComponentRegistry::Get().Find("AwakeProbe");
    ASSERT_TRUE(info->create);
    auto comp = info->create();
    ASSERT_TRUE(info->FindField("value")->set(*comp, FieldValue(7.0f)));
    auto& attached = static_cast<AwakeProbe&>(go.AddComponent(std::move(comp)));
    EXPECT_FLOAT_EQ(attached.seenInAwake, 7.0f) << "Awake ma widziec wartosci ustawione przed dolaczeniem";
    EXPECT_EQ(go.GetComponent<AwakeProbe>(), &attached);
}

TEST(Reflection, AddComponentRejectsTransformAndNull) {
    wz::Scene scene;
    auto& go = scene.CreateGameObject();
    EXPECT_THROW(go.AddComponent(std::unique_ptr<wz::Component>()), std::invalid_argument);
    EXPECT_THROW(go.AddComponent(std::make_unique<wz::Transform>()), std::invalid_argument);
    EXPECT_FALSE(ComponentRegistry::Get().Find<wz::Transform>()->create);
}

TEST(Reflection, NonDefaultConstructibleHasNoFactory) {
    const auto* info = ComponentRegistry::Get().Find("NoDefault");
    ASSERT_NE(info, nullptr);
    EXPECT_FALSE(info->addTo);
    EXPECT_FALSE(info->create);
    ASSERT_EQ(info->fields.size(), 1u);
}

TEST(Reflection, EmptyComponentHasNoFields) {
    const auto* info = ComponentRegistry::Get().Find("Empty");
    ASSERT_NE(info, nullptr);
    EXPECT_TRUE(info->fields.empty());
}

TEST(Reflection, UnknownLookupsReturnNull) {
    EXPECT_EQ(ComponentRegistry::Get().Find("NieMaTakiego"), nullptr);
    EXPECT_EQ(ComponentRegistry::Get().Find("Sample")->FindField("brak"), nullptr);
}

TEST(Reflection, DuplicateInGlobalRegistryIsRejectedNotFatal) {
    // Sprawdzamy zachowanie domyślne (nie-strict), także w buildzie z WZ_REGISTRY_STRICT.
    auto& reg = ComponentRegistry::Get();
    const bool wasStrict = reg.IsStrict();
    reg.SetStrict(false);
    testing::internal::CaptureStderr();
    EXPECT_EQ(ComponentRegistry::Get().Register(wz::TypeInfo("Transform", std::type_index(typeid(int)))), nullptr);
    EXPECT_EQ(ComponentRegistry::Get().Register(wz::TypeInfo("InnaNazwa", std::type_index(typeid(Sample)))), nullptr);
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("'Transform' zarejestrowany dwa razy"), std::string::npos) << err;
    EXPECT_EQ(ComponentRegistry::Get().Find("Transform")->type, std::type_index(typeid(wz::Transform)));
    EXPECT_EQ(ComponentRegistry::Get().Find("InnaNazwa"), nullptr);
    reg.SetStrict(wasStrict);
}

TEST(Reflection, AllIsSortedByName) {
    const auto all = ComponentRegistry::Get().All();
    ASSERT_GE(all.size(), 4u);
    for (std::size_t i = 1; i < all.size(); ++i) EXPECT_LT(all[i - 1]->name, all[i]->name);
}
