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
#include "render/MeshRenderer.h"
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
    EXPECT_FALSE(guard::IsFinite(glm::vec4(1, 2, 3, kInf)));
    EXPECT_FALSE(guard::IsFinite(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_TRUE(guard::IsFinite(1e300));
    EXPECT_FALSE(guard::IsFinite(glm::quat(kNaN, 0, 0, 0)));
}

TEST_F(FloatGuardTest, CoreCompatibleSanitizeMatchesApiFrom4) {
    // Te same sygnatury co wz::Sanitize* z #4 (56c7658) - po rebase zamiana 1:1.
    EXPECT_FLOAT_EQ(guard::SanitizeFloat(kNaN, 2.0f, "test.float"), 2.0f);
    EXPECT_FLOAT_EQ(guard::SanitizeFloat(3.0f, 2.0f, nullptr), 3.0f);
    EXPECT_DOUBLE_EQ(guard::SanitizeDouble(-std::numeric_limits<double>::infinity(), 1.5, "test.double"), 1.5);
    ExpectVecNear(guard::SanitizeVec3({1, kInf, 3}, {0, 0, 0}, "test.vec3"), {0, 0, 0});
    const glm::quat q = guard::SanitizeQuat(glm::quat(1, kNaN, 0, 0), glm::quat(1, 0, 0, 0), "test.quat");
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_EQ(guard::NonFiniteWarningCount(), 0); // API z #4 nie korzysta z rejestru "raz na obiekt"
}

TEST_F(FloatGuardTest, LightParametersFallBackToDefaultsAndWarnOncePerObject) {
    Scene s;
    auto& p = s.CreateGameObject("Lamp").AddComponent<PointLight>();
    p.intensity = kNaN;
    p.range = kInf;
    p.color = {kNaN, 0.5f, -kInf};
    EXPECT_FLOAT_EQ(p.GetShaderIntensity(), Light::kDefaultIntensity);
    EXPECT_FLOAT_EQ(p.GetShaderRange(), PointLight::kDefaultRange);
    ExpectVecNear(p.GetShaderColor(), glm::vec3(1.0f, 0.5f, 1.0f)); // kanal po kanale, poprawny 0.5 zostaje
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
    // Kolor kanal po kanale: NaN i Inf -> 1, poprawny kanal 0.25 zostaje (zgodnie z testami #19).
    EXPECT_EQ(m.GetShaderBaseColor(), glm::vec4(1.0f, 0.25f, 1.0f, 1.0f));
    ExpectVecNear(m.GetShaderEmissive(), glm::vec3(0.0f, 0.0f, 2.0f)); // kanal po kanale
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

// --- Ostatnia dobra wartosc (preferencja testerow): bez skokow do domyslnej ---

TEST_F(FloatGuardTest, LightKeepsLastGoodValueInsteadOfDefault) {
    Scene s;
    auto& sp = s.CreateGameObject("Spot").AddComponent<SpotLight>();
    sp.intensity = 7.0f;
    sp.color = {0.2f, 0.4f, 0.6f};
    sp.range = 3.0f;
    sp.innerAngle = 12.0f;
    sp.outerAngle = 25.0f;
    // Klatka 1: wszystko dobre - zapamietane.
    EXPECT_FLOAT_EQ(sp.GetShaderIntensity(), 7.0f);
    ExpectVecNear(sp.GetShaderColor(), {0.2f, 0.4f, 0.6f});
    EXPECT_FLOAT_EQ(sp.GetShaderRange(), 3.0f);
    EXPECT_FLOAT_EQ(sp.GetShaderInnerAngle(), 12.0f);
    EXPECT_FLOAT_EQ(sp.GetShaderOuterAngle(), 25.0f);
    // Klatka 2: smieci z zewnatrz - zostaja OSTATNIE dobre wartosci, nie domyslne.
    sp.intensity = kNaN;
    sp.color = {kInf, 0.9f, kNaN};
    sp.range = -kInf;
    sp.innerAngle = kNaN;
    sp.outerAngle = kInf;
    EXPECT_FLOAT_EQ(sp.GetShaderIntensity(), 7.0f);
    ExpectVecNear(sp.GetShaderColor(), {0.2f, 0.9f, 0.6f}); // poprawny kanal g idzie dalej
    EXPECT_FLOAT_EQ(sp.GetShaderRange(), 3.0f);
    EXPECT_FLOAT_EQ(sp.GetShaderInnerAngle(), 12.0f);
    EXPECT_FLOAT_EQ(sp.GetShaderOuterAngle(), 25.0f);
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1);
    // Klatka 3: nowa dobra wartosc staje sie "ostatnia dobra".
    sp.intensity = 2.0f;
    EXPECT_FLOAT_EQ(sp.GetShaderIntensity(), 2.0f);
    sp.intensity = kInf;
    EXPECT_FLOAT_EQ(sp.GetShaderIntensity(), 2.0f);
    // Pakowanie swiatel tez uzywa ostatnich dobrych wartosci (bez blysku jasnosci).
    sp.intensity = kNaN;
    const GpuLightBlock b = PackLights(CollectLights(s, glm::vec3(0.0f)), glm::vec3(0.0f));
    ASSERT_EQ(b.counts.z, 1);
    ExpectVecNear(glm::vec3(b.spots[0].colorIntensity), glm::vec3(0.2f, 0.9f, 0.6f) * 2.0f);
    EXPECT_FLOAT_EQ(b.spots[0].positionRange.w, 3.0f);
    EXPECT_FLOAT_EQ(b.spots[0].cosInner.x, std::cos(glm::radians(12.0f)));
}

TEST_F(FloatGuardTest, MaterialKeepsLastGoodValueThenClamps) {
    Material m;
    m.metallic = 0.8f;
    m.roughness = 0.3f;
    m.baseColor = {0.1f, 0.2f, 0.3f, 0.4f};
    m.emissive = {1.0f, 2.0f, 3.0f};
    (void)m.GetShaderMetallic();
    (void)m.GetShaderRoughness();
    (void)m.GetShaderBaseColor();
    (void)m.GetShaderEmissive();
    m.metallic = kNaN;
    m.roughness = -kInf;
    m.baseColor = {kNaN, 0.7f, kInf, -kInf};
    m.emissive = {kNaN, 5.0f, kNaN};
    EXPECT_FLOAT_EQ(m.GetShaderMetallic(), 0.8f);
    EXPECT_FLOAT_EQ(m.GetShaderRoughness(), 0.3f);
    EXPECT_EQ(m.GetShaderBaseColor(), glm::vec4(0.1f, 0.7f, 0.3f, 0.4f));
    ExpectVecNear(m.GetShaderEmissive(), {1.0f, 5.0f, 3.0f});
    // Ostatnia dobra wartosc poza zakresem jest i tak przycinana (najpierw sanityzacja, potem clamp).
    m.metallic = 4.0f;
    EXPECT_FLOAT_EQ(m.GetShaderMetallic(), 1.0f);
    m.metallic = kNaN;
    EXPECT_FLOAT_EQ(m.GetShaderMetallic(), 1.0f);
}

TEST_F(FloatGuardTest, NeverGoodFallsBackToDefaultPerChannel) {
    guard::LastGoodVec3 last;
    const glm::vec3 v = guard::SanitizeVec3KeepLast({kNaN, 2.0f, kInf}, last, glm::vec3(9.0f), this, "t", "v");
    ExpectVecNear(v, {9.0f, 2.0f, 9.0f});
    EXPECT_TRUE(last.valid[1]);
    EXPECT_FALSE(last.valid[0]);
    // Nastepnym razem kanal 1 pamieta 2, kanaly 0 i 2 wciaz domyslne.
    ExpectVecNear(guard::SanitizeVec3KeepLast({kNaN, kNaN, 4.0f}, last, glm::vec3(9.0f), this, "t", "v"), {9.0f, 2.0f, 4.0f});
    guard::LastGoodFloat f;
    EXPECT_FLOAT_EQ(guard::SanitizeFloatKeepLast(kNaN, f, 1.5f, this, "t", "f"), 1.5f);
    EXPECT_FLOAT_EQ(guard::SanitizeFloatKeepLast(0.25f, f, 1.5f, this, "t", "f"), 0.25f);
    EXPECT_FLOAT_EQ(guard::SanitizeFloatKeepLast(-kInf, f, 1.5f, this, "t", "f"), 0.25f);
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1); // jeden obiekt (this) = jedno ostrzezenie
}

TEST_F(FloatGuardTest, MeshRendererTintKeepsLastGoodChannels) {
    MeshRenderer mr;
    EXPECT_EQ(mr.GetShaderColor(), glm::vec4(1.0f));
    mr.color = {0.3f, 0.6f, 0.9f, 1.0f};
    EXPECT_EQ(mr.GetShaderColor(), glm::vec4(0.3f, 0.6f, 0.9f, 1.0f));
    mr.color = {kNaN, 0.1f, kInf, -kInf};
    EXPECT_EQ(mr.GetShaderColor(), glm::vec4(0.3f, 0.1f, 0.9f, 1.0f)); // poprawny kanal g idzie dalej
    EXPECT_EQ(guard::NonFiniteWarningCount(), 1);
}
