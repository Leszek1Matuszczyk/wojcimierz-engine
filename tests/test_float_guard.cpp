// ============================================================
//  Wojcimierz Engine: tests/test_float_guard.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Straz NaN/Inf na wejsciach renderera (swiatla, materialy).
#include "TestUtils.h"
#include "render/Light.h"
#include "render/Lighting.h"
#include "render/Material.h"
#include "render/detail/FloatGuard.h"
#include "scene/Scene.h"

#include <cmath>
#include <limits>

using namespace wz;
using namespace wz::lighting;
namespace guard = wz::render_detail;

namespace {
const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

bool AllFinite(const GpuLightBlock& b) {
    auto ok = [](const glm::vec4& v) { return guard::IsFinite(v); };
    if (!ok(b.ambientColor) || !ok(b.directionalDirection) || !ok(b.directionalColor)) return false;
    for (const auto& p : b.points)
        if (!ok(p.positionRange) || !ok(p.colorIntensity)) return false;
    for (const auto& s : b.spots)
        if (!ok(s.positionRange) || !ok(s.directionCosOuter) || !ok(s.colorIntensity) || !ok(s.cosInner)) return false;
    return true;
}

class FloatGuardTest : public ::testing::Test {
protected:
    void SetUp() override { guard::ResetNonFiniteWarnings(); }
};
} // namespace

TEST_F(FloatGuardTest, IsFiniteDetectsNaNAndInf) {
    EXPECT_TRUE(guard::IsFinite(0.0f));
    EXPECT_TRUE(guard::IsFinite(-3.0e38f));
    EXPECT_TRUE(guard::IsFinite(std::numeric_limits<float>::denorm_min()));
    EXPECT_FALSE(guard::IsFinite(kNaN));
    EXPECT_FALSE(guard::IsFinite(kInf));
    EXPECT_FALSE(guard::IsFinite(-kInf));
    EXPECT_FALSE(guard::IsFinite(glm::vec3(0, kNaN, 0)));
    EXPECT_TRUE(guard::IsFinite(glm::vec4(1, 2, 3, 4)));
}

TEST_F(FloatGuardTest, LightParametersFallBackToDefaultsAndWarnOncePerObject) {
    Scene s;
    auto& p = s.CreateGameObject("Lamp").AddComponent<PointLight>();
    p.intensity = kNaN;
    p.range = kInf;
    p.color = {kNaN, 0.5f, -kInf};
    EXPECT_FLOAT_EQ(p.GetShaderIntensity(), Light::kDefaultIntensity);
    EXPECT_FLOAT_EQ(p.GetShaderRange(), PointLight::kDefaultRange);
    ExpectVecNear(p.GetShaderColor(), {1.0f, 0.5f, 1.0f}); // poprawne skladowe zostaja
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1);           // trzy zle pola, jedno ostrzezenie
    for (int i = 0; i < 10; ++i) (void)p.GetShaderIntensity(); // "kolejne klatki"
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1);

    auto& sp = s.CreateGameObject("Spot").AddComponent<SpotLight>();
    sp.innerAngle = kNaN;
    sp.outerAngle = -kInf;
    sp.range = kNaN;
    EXPECT_FLOAT_EQ(sp.GetShaderInnerAngle(), SpotLight::kDefaultInnerAngle);
    EXPECT_FLOAT_EQ(sp.GetShaderOuterAngle(), SpotLight::kDefaultOuterAngle);
    EXPECT_FLOAT_EQ(sp.GetShaderRange(), SpotLight::kDefaultRange);
    EXPECT_EQ(guard::NonFiniteWarningCount(), 2); // drugi obiekt = drugie ostrzezenie

    // Poprawne wartosci przechodza bez zmian i bez ostrzezen.
    auto& ok = s.CreateGameObject("Ok").AddComponent<PointLight>();
    ok.intensity = 3.0f;
    ok.range = 7.0f;
    EXPECT_FLOAT_EQ(ok.GetShaderIntensity(), 3.0f);
    EXPECT_FLOAT_EQ(ok.GetShaderRange(), 7.0f);
    EXPECT_EQ(guard::NonFiniteWarningCount(), 2);
}

TEST_F(FloatGuardTest, SpotConeIsFiniteForNonFiniteAngles) {
    const SpotCone c = ComputeSpotCone(kNaN, kInf);
    EXPECT_TRUE(std::isfinite(c.cosInner));
    EXPECT_TRUE(std::isfinite(c.cosOuter));
    EXPECT_GE(c.cosInner, c.cosOuter);
}

TEST_F(FloatGuardTest, PackedLightBlockIsFiniteWithNonFiniteParameters) {
    Scene s;
    auto& sun = s.CreateGameObject("Sun").AddComponent<DirectionalLight>();
    sun.intensity = kInf;
    sun.color = glm::vec3(kNaN);
    GameObject& lampGo = s.CreateGameObject("Lamp");
    lampGo.transform().SetPosition({1, 0, 0});
    auto& lamp = lampGo.AddComponent<PointLight>();
    lamp.range = kNaN;
    lamp.intensity = -kInf;
    auto& spot = s.CreateGameObject("Spot").AddComponent<SpotLight>();
    spot.innerAngle = kNaN;
    spot.outerAngle = kNaN;
    spot.range = kInf;
    spot.color = {kInf, 1, 1};

    const LightCollection lc = CollectLights(s, glm::vec3(0.0f));
    EXPECT_EQ(lc.directional.size(), 1u);
    EXPECT_EQ(lc.points.size(), 1u);
    EXPECT_EQ(lc.spots.size(), 1u);
    const GpuLightBlock b = PackLights(lc, glm::vec3(0.1f));
    EXPECT_TRUE(AllFinite(b));
    ExpectVecNear(glm::vec3(b.directionalColor), glm::vec3(Light::kDefaultIntensity));
    EXPECT_FLOAT_EQ(b.points[0].positionRange.w, PointLight::kDefaultRange);
    EXPECT_EQ(guard::NonFiniteWarningCount(), 3);
}

TEST_F(FloatGuardTest, LightsWithNonFiniteTransformAreSkipped) {
    Scene s;
    GameObject& badPos = s.CreateGameObject("BadPos");
    badPos.transform().SetPosition({kNaN, 0, 0});
    badPos.AddComponent<PointLight>();
    GameObject& badSpot = s.CreateGameObject("BadSpot");
    badSpot.transform().SetPosition({0, kInf, 0});
    badSpot.AddComponent<SpotLight>();
    GameObject& badDir = s.CreateGameObject("BadDir");
    badDir.transform().SetLocalRotation(glm::quat(kNaN, 0, 0, 0));
    badDir.AddComponent<DirectionalLight>();
    GameObject& good = s.CreateGameObject("Good");
    good.transform().SetPosition({1, 0, 0});
    good.AddComponent<PointLight>();

    const LightCollection lc = CollectLights(s, glm::vec3(0.0f));
    EXPECT_TRUE(lc.directional.empty());
    ASSERT_EQ(lc.points.size(), 1u);
    EXPECT_EQ(lc.points[0]->gameObject().GetName(), "Good");
    EXPECT_TRUE(lc.spots.empty());
    EXPECT_TRUE(AllFinite(PackLights(lc, glm::vec3(0.0f))));
    EXPECT_EQ(guard::NonFiniteWarningCount(), 3);
    (void)CollectLights(s, glm::vec3(0.0f)); // druga klatka - bez nowych ostrzezen
    EXPECT_EQ(guard::NonFiniteWarningCount(), 3);
}

TEST_F(FloatGuardTest, MaterialParametersFallBackToDefaults) {
    Material m;
    m.name = "Zly";
    m.metallic = kNaN;
    m.roughness = kInf;
    m.baseColor = {kNaN, 0.25f, kInf, 1.0f};
    m.emissive = {0.0f, kNaN, 2.0f};
    EXPECT_FLOAT_EQ(m.GetShaderMetallic(), Material::kDefaultMetallic);
    EXPECT_FLOAT_EQ(m.GetShaderRoughness(), Material::kDefaultRoughness);
    const glm::vec4 bc = m.GetShaderBaseColor();
    EXPECT_FLOAT_EQ(bc.r, 1.0f);
    EXPECT_FLOAT_EQ(bc.g, 0.25f);
    EXPECT_FLOAT_EQ(bc.b, 1.0f);
    ExpectVecNear(m.GetShaderEmissive(), {0.0f, 0.0f, 2.0f});
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1);

    Material good;
    good.metallic = 0.7f;
    good.emissive = {1, 2, 3};
    EXPECT_FLOAT_EQ(good.GetShaderMetallic(), 0.7f);
    ExpectVecNear(good.GetShaderEmissive(), {1, 2, 3});
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1);
}

TEST_F(FloatGuardTest, DestroyedObjectForgetsItsWarning) {
    const void* addr = nullptr;
    {
        Material m;
        m.metallic = kNaN;
        (void)m.GetShaderMetallic();
        addr = &m;
    }
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1);
    // Ten sam adres po zniszczeniu nie jest juz "wyciszony".
    guard::WarnNonFiniteOnce(addr, "nowy", "metallic", "uzyto 0");
    EXPECT_EQ(guard::NonFiniteWarningCount(), 2);
}
