// ============================================================
//  Wojcimierz Engine: tests/test_render_registration.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Refleksja komponentow renderera: tworzenie po nazwie z biblioteki statycznej (wz_core)
// po jawnej rejestracji oraz round-trip get/set przez FieldInfo ze strzezonymi setterami.
#include "render/Light.h"
#include "render/MeshRenderer.h"
#include "render/RenderComponents.h"
#include "scene/ComponentRegistry.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using wz::ComponentRegistry;
using wz::FieldValue;
using wz::TypeInfo;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

const TypeInfo& Info(const char* name) {
    wz::RegisterRenderComponents();
    const TypeInfo* info = ComponentRegistry::Get().Find(name);
    EXPECT_NE(info, nullptr) << name;
    return *info;
}

template <typename V>
V Get(const TypeInfo& info, const wz::Component& c, const char* field) {
    const wz::FieldInfo* f = info.FindField(field);
    EXPECT_NE(f, nullptr) << info.name << "." << field;
    return std::get<V>(f->get(c));
}

bool Set(const TypeInfo& info, wz::Component& c, const char* field, FieldValue v) {
    const wz::FieldInfo* f = info.FindField(field);
    EXPECT_NE(f, nullptr) << info.name << "." << field;
    return f && f->set(c, v);
}

} // namespace

TEST(RenderRegistration, CreateByNameFromStaticLibrary) {
    wz::RegisterRenderComponents();
    wz::RegisterRenderComponents(); // idempotentne
    for (const char* name : {"MeshRenderer", "DirectionalLight", "PointLight", "SpotLight"}) {
        const TypeInfo* info = ComponentRegistry::Get().Find(name);
        ASSERT_NE(info, nullptr) << name;
        EXPECT_EQ(info->cppName, std::string("wz::") + name);
        auto created = ComponentRegistry::Get().Create(name);
        ASSERT_NE(created, nullptr) << name;
        EXPECT_EQ(ComponentRegistry::Get().FindFor(*created), info) << name;
    }
    EXPECT_NE(dynamic_cast<wz::SpotLight*>(ComponentRegistry::Get().Create("SpotLight").get()), nullptr);
    EXPECT_EQ(ComponentRegistry::Get().Find("Light"), nullptr) << "Light jest abstrakcyjny - nie rejestrujemy";
}

TEST(RenderRegistration, IdempotentRegistrationDoesNotGrowRegistry) {
    wz::RegisterRenderComponents();
    const std::size_t before = ComponentRegistry::Get().Size();
    wz::RegisterRenderComponents();
    EXPECT_EQ(ComponentRegistry::Get().Size(), before);
}

TEST(RenderRegistration, LightFieldsMatchSpecification) {
    const TypeInfo& spot = Info("SpotLight");
    for (const char* f : {"color", "intensity", "range", "innerAngle", "outerAngle", "castShadows", "shadowStrength",
                          "shadowBias", "normalBias"})
        EXPECT_NE(spot.FindField(f), nullptr) << f;
    const TypeInfo& point = Info("PointLight");
    EXPECT_NE(point.FindField("range"), nullptr);
    EXPECT_EQ(point.FindField("innerAngle"), nullptr);
    const TypeInfo& dir = Info("DirectionalLight");
    EXPECT_NE(dir.FindField("shadowBias"), nullptr);
    EXPECT_EQ(dir.FindField("range"), nullptr);
}

TEST(RenderRegistration, MeshRendererReflectsScalarsOnly) {
    const TypeInfo& mr = Info("MeshRenderer");
    for (const char* f : {"color", "unlit", "cullBackFaces", "castShadows", "receiveShadows"})
        EXPECT_NE(mr.FindField(f), nullptr) << f;
    for (const char* f : {"mesh", "materials", "shader", "cpuMesh"}) EXPECT_EQ(mr.FindField(f), nullptr) << f;
}

TEST(RenderRegistration, SpotLightRoundTrip) {
    const TypeInfo& info = Info("SpotLight");
    auto c = ComponentRegistry::Get().Create("SpotLight");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(Set(info, *c, "color", glm::vec3(0.25f, 0.5f, 2.0f)));
    EXPECT_TRUE(Set(info, *c, "intensity", 3.5f));
    EXPECT_TRUE(Set(info, *c, "range", 42.0f));
    EXPECT_TRUE(Set(info, *c, "innerAngle", 12.0f));
    EXPECT_TRUE(Set(info, *c, "outerAngle", 40.0f));
    EXPECT_TRUE(Set(info, *c, "castShadows", true));
    EXPECT_TRUE(Set(info, *c, "shadowStrength", 0.75f));
    EXPECT_TRUE(Set(info, *c, "shadowBias", 0.1f));
    EXPECT_TRUE(Set(info, *c, "normalBias", 1.5f));

    EXPECT_EQ(Get<glm::vec3>(info, *c, "color"), glm::vec3(0.25f, 0.5f, 2.0f));
    EXPECT_FLOAT_EQ(Get<float>(info, *c, "intensity"), 3.5f);
    EXPECT_FLOAT_EQ(Get<float>(info, *c, "range"), 42.0f);
    EXPECT_FLOAT_EQ(Get<float>(info, *c, "innerAngle"), 12.0f);
    EXPECT_FLOAT_EQ(Get<float>(info, *c, "outerAngle"), 40.0f);
    EXPECT_TRUE(Get<bool>(info, *c, "castShadows"));
    EXPECT_FLOAT_EQ(Get<float>(info, *c, "shadowStrength"), 0.75f);
    EXPECT_FLOAT_EQ(Get<float>(info, *c, "shadowBias"), 0.1f);
    EXPECT_FLOAT_EQ(Get<float>(info, *c, "normalBias"), 1.5f);

    // Pola komponentu widza to samo, co refleksja.
    const auto& spot = static_cast<const wz::SpotLight&>(*c);
    EXPECT_FLOAT_EQ(spot.range, 42.0f);
    EXPECT_TRUE(spot.castShadows);

    // Zly typ wartosci jest odrzucany i nie zmienia pola.
    EXPECT_FALSE(Set(info, *c, "intensity", std::int32_t{7}));
    EXPECT_FLOAT_EQ(spot.intensity, 3.5f);
}

TEST(RenderRegistration, MeshRendererRoundTrip) {
    const TypeInfo& info = Info("MeshRenderer");
    auto c = ComponentRegistry::Get().Create("MeshRenderer");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(Set(info, *c, "color", glm::vec4(0.1f, 0.2f, 0.3f, 0.4f)));
    EXPECT_TRUE(Set(info, *c, "unlit", true));
    EXPECT_TRUE(Set(info, *c, "cullBackFaces", false));
    EXPECT_TRUE(Set(info, *c, "castShadows", false));
    EXPECT_TRUE(Set(info, *c, "receiveShadows", false));
    EXPECT_EQ(Get<glm::vec4>(info, *c, "color"), glm::vec4(0.1f, 0.2f, 0.3f, 0.4f));
    EXPECT_TRUE(Get<bool>(info, *c, "unlit"));
    EXPECT_FALSE(Get<bool>(info, *c, "cullBackFaces"));
    EXPECT_FALSE(Get<bool>(info, *c, "castShadows"));
    EXPECT_FALSE(Get<bool>(info, *c, "receiveShadows"));
}

// Strzezone settery: najpierw sanityzacja (NaN/Inf -> bez zmian), potem clamp.
TEST(RenderRegistration, GuardedSettersSanitizeThenClamp) {
    const TypeInfo& info = Info("SpotLight");
    auto c = ComponentRegistry::Get().Create("SpotLight");
    ASSERT_NE(c, nullptr);
    const auto& spot = static_cast<const wz::SpotLight&>(*c);
    Set(info, *c, "intensity", 2.0f);
    testing::internal::CaptureStderr();
    Set(info, *c, "intensity", kNaN);
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_FLOAT_EQ(spot.intensity, 2.0f) << "NaN zostawia biezaca wartosc";
    EXPECT_NE(err.find("Light.intensity"), std::string::npos) << err;

    testing::internal::CaptureStderr();
    Set(info, *c, "intensity", kInf);
    EXPECT_FLOAT_EQ(spot.intensity, 2.0f) << "Inf nie staje sie kMaxIntensity - sanityzacja przed clampem";
    Set(info, *c, "intensity", std::numeric_limits<float>::max());
    EXPECT_FLOAT_EQ(spot.intensity, wz::Light::kMaxIntensity);
    Set(info, *c, "intensity", -5.0f);
    EXPECT_FLOAT_EQ(spot.intensity, 0.0f);

    Set(info, *c, "color", glm::vec3(0.5f));
    Set(info, *c, "color", glm::vec3(1.0f, kNaN, 1.0f));
    EXPECT_EQ(spot.color, glm::vec3(0.5f)) << "wz::SanitizeVec3 odrzuca caly wektor";
    Set(info, *c, "color", glm::vec3(-1.0f, 2.0f, 1e30f));
    EXPECT_EQ(spot.color, glm::vec3(0.0f, 2.0f, wz::Light::kMaxColor));

    Set(info, *c, "range", 5.0f);
    Set(info, *c, "range", -kInf);
    EXPECT_FLOAT_EQ(spot.range, 5.0f);
    Set(info, *c, "range", -3.0f);
    EXPECT_FLOAT_EQ(spot.range, 0.0f) << "<= 0 to swiatlo wylaczone";
    Set(info, *c, "range", 1e-9f);
    EXPECT_FLOAT_EQ(spot.range, wz::kMinLightRange);
    Set(info, *c, "range", 1e30f);
    EXPECT_FLOAT_EQ(spot.range, wz::kMaxLightRange);

    Set(info, *c, "outerAngle", 120.0f);
    EXPECT_FLOAT_EQ(spot.outerAngle, wz::SpotLight::kMaxAngle);
    Set(info, *c, "innerAngle", kNaN);
    EXPECT_FLOAT_EQ(spot.innerAngle, wz::SpotLight::kDefaultInnerAngle);
    Set(info, *c, "innerAngle", 0.0f);
    EXPECT_FLOAT_EQ(spot.innerAngle, wz::SpotLight::kMinAngle);

    Set(info, *c, "shadowStrength", 3.0f);
    EXPECT_FLOAT_EQ(spot.shadowStrength, 1.0f);
    Set(info, *c, "shadowBias", 1e9f);
    EXPECT_FLOAT_EQ(spot.shadowBias, wz::Light::kMaxShadowBias);
    Set(info, *c, "normalBias", kNaN);
    EXPECT_FLOAT_EQ(spot.normalBias, wz::Light::kDefaultNormalBias);
    Set(info, *c, "normalBias", -1.0f);
    EXPECT_FLOAT_EQ(spot.normalBias, 0.0f);
    testing::internal::GetCapturedStderr();
    for (float v : {spot.intensity, spot.range, spot.innerAngle, spot.outerAngle, spot.shadowStrength, spot.shadowBias,
                    spot.normalBias})
        EXPECT_TRUE(std::isfinite(v));
}

TEST(RenderRegistration, MeshRendererTintSetterSanitizesPerChannel) {
    const TypeInfo& info = Info("MeshRenderer");
    auto c = ComponentRegistry::Get().Create("MeshRenderer");
    ASSERT_NE(c, nullptr);
    const auto& mr = static_cast<const wz::MeshRenderer&>(*c);
    Set(info, *c, "color", glm::vec4(0.5f));
    testing::internal::CaptureStderr();
    Set(info, *c, "color", glm::vec4(kNaN, 0.25f, kInf, -1.0f));
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(mr.color, glm::vec4(0.5f, 0.25f, 0.5f, 0.0f)) << "zly kanal zostaje, dobre przechodza, potem clamp";
}
