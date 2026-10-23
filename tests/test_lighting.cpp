// ============================================================
//  Wojcimierz Engine: tests/test_lighting.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "render/Light.h"
#include "render/Lighting.h"
#include "scene/Scene.h"

#include <cmath>
#include <string>

using namespace wz;
using namespace wz::lighting;

namespace {
PointLight& AddPoint(Scene& s, const std::string& name, glm::vec3 pos, float range = 5.0f, float intensity = 1.0f) {
    GameObject& go = s.CreateGameObject(name);
    go.transform().SetPosition(pos);
    auto& l = go.AddComponent<PointLight>();
    l.range = range;
    l.intensity = intensity;
    return l;
}
} // namespace

// --- matematyka swiatel (ta sama co w shaderze PBR) ---

TEST(LightMath, DistanceAttenuationIsInverseSquareNearSource) {
    // Daleko od granicy zasiegu okienko ~1, wiec zostaje 1/d^2.
    EXPECT_NEAR(DistanceAttenuation(1.0f, 1000.0f), 1.0f, 1e-4f);
    EXPECT_NEAR(DistanceAttenuation(2.0f, 1000.0f), 0.25f, 1e-4f);
    EXPECT_NEAR(DistanceAttenuation(4.0f, 1000.0f) / DistanceAttenuation(2.0f, 1000.0f), 0.25f, 1e-3f);
}

TEST(LightMath, DistanceAttenuationReachesZeroAtRangeAndIsMonotonic) {
    EXPECT_FLOAT_EQ(DistanceAttenuation(10.0f, 10.0f), 0.0f);
    EXPECT_FLOAT_EQ(DistanceAttenuation(12.0f, 10.0f), 0.0f);
    EXPECT_FLOAT_EQ(DistanceAttenuation(1.0f, 0.0f), 0.0f);
    EXPECT_GT(DistanceAttenuation(9.9f, 10.0f), 0.0f);
    float prev = DistanceAttenuation(0.1f, 10.0f);
    for (float d = 0.2f; d < 10.0f; d += 0.1f) {
        const float a = DistanceAttenuation(d, 10.0f);
        EXPECT_LE(a, prev) << "d = " << d;
        prev = a;
    }
}

TEST(LightMath, DistanceAttenuationIsFiniteAtZeroDistance) {
    const float a = DistanceAttenuation(0.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(a));
    EXPECT_FLOAT_EQ(a, 1.0f / 1e-4f);
}

TEST(LightMath, SpotAttenuationFullInsideInnerZeroOutsideOuter) {
    const SpotCone c = ComputeSpotCone(20.0f, 30.0f);
    EXPECT_NEAR(c.cosInner, std::cos(glm::radians(20.0f)), 1e-6f);
    EXPECT_NEAR(c.cosOuter, std::cos(glm::radians(30.0f)), 1e-6f);
    EXPECT_FLOAT_EQ(SpotAttenuation(1.0f, c.cosInner, c.cosOuter), 1.0f);                         // na osi
    EXPECT_FLOAT_EQ(SpotAttenuation(std::cos(glm::radians(19.0f)), c.cosInner, c.cosOuter), 1.0f); // w inner
    EXPECT_FLOAT_EQ(SpotAttenuation(std::cos(glm::radians(31.0f)), c.cosInner, c.cosOuter), 0.0f); // za outer
    const float mid = SpotAttenuation(std::cos(glm::radians(25.0f)), c.cosInner, c.cosOuter);
    EXPECT_GT(mid, 0.0f);
    EXPECT_LT(mid, 1.0f);
}

TEST(LightMath, SpotAttenuationDecreasesWithAngle) {
    const SpotCone c = ComputeSpotCone(10.0f, 40.0f);
    float prev = 2.0f;
    for (float deg = 0.0f; deg <= 45.0f; deg += 1.0f) {
        const float a = SpotAttenuation(std::cos(glm::radians(deg)), c.cosInner, c.cosOuter);
        EXPECT_LE(a, prev) << deg;
        prev = a;
    }
}

TEST(LightMath, SpotConeIsSanitized) {
    // inner > outer -> inner obciete do outer; outer >= 90 -> SpotLight::kMaxAngle (89.9).
    const SpotCone a = ComputeSpotCone(50.0f, 30.0f);
    EXPECT_FLOAT_EQ(a.cosInner, a.cosOuter);
    const SpotCone b = ComputeSpotCone(10.0f, 120.0f);
    EXPECT_NEAR(b.cosOuter, std::cos(glm::radians(SpotLight::kMaxAngle)), 1e-6f);
    EXPECT_GT(b.cosOuter, 0.0f);
    // Zdegenerowany stozek (inner == outer) nie dzieli przez zero.
    EXPECT_TRUE(std::isfinite(SpotAttenuation(a.cosOuter, a.cosInner, a.cosOuter)));
}

// --- komponenty ---

TEST(LightComponents, DefaultsAndTypes) {
    Scene s;
    auto& d = s.CreateGameObject("Sun").AddComponent<DirectionalLight>();
    auto& p = s.CreateGameObject("Lamp").AddComponent<PointLight>();
    auto& sp = s.CreateGameObject("Spot").AddComponent<SpotLight>();
    EXPECT_EQ(d.GetType(), LightType::Directional);
    EXPECT_EQ(p.GetType(), LightType::Point);
    EXPECT_EQ(sp.GetType(), LightType::Spot);
    ExpectVecNear(d.color, glm::vec3(1.0f));
    EXPECT_FLOAT_EQ(d.intensity, 1.0f);
    EXPECT_FLOAT_EQ(p.range, 10.0f);
    EXPECT_FLOAT_EQ(sp.innerAngle, 20.0f);
    EXPECT_FLOAT_EQ(sp.outerAngle, 30.0f);
    EXPECT_TRUE(p.IsContributing());
    p.intensity = 0.0f;
    EXPECT_FALSE(p.IsContributing());
    p.intensity = 1.0f;
    p.color = glm::vec3(0.0f);
    EXPECT_FALSE(p.IsContributing());
}

// --- zbieranie swiatel i limity ---

TEST(LightCollection, CollectsAllTypesAndSkipsInactive) {
    Scene s;
    s.CreateGameObject("Sun").AddComponent<DirectionalLight>();
    AddPoint(s, "P0", {1, 0, 0});
    AddPoint(s, "Off", {2, 0, 0}).enabled = false;
    AddPoint(s, "Hidden", {3, 0, 0}).gameObject().SetActive(false);
    AddPoint(s, "Dark", {4, 0, 0}, 5.0f, 0.0f);
    AddPoint(s, "NoRange", {5, 0, 0}, 0.0f);
    s.CreateGameObject("Spot").AddComponent<SpotLight>();
    const LightCollection lc = CollectLights(s, glm::vec3(0.0f));
    EXPECT_EQ(lc.directional.size(), 1u);
    ASSERT_EQ(lc.points.size(), 1u);
    EXPECT_EQ(lc.points[0]->gameObject().GetName(), "P0");
    EXPECT_EQ(lc.spots.size(), 1u);
    EXPECT_EQ(lc.droppedPoint, 0);
}

TEST(LightCollection, RespectsHardLimitsAndKeepsNearest) {
    Scene s;
    // 12 swiatel punktowych w odleglosciach 10, 20, ..., 120 od kamery, zasieg 5.
    for (int i = 11; i >= 0; --i) AddPoint(s, "P" + std::to_string(i), {10.0f * (i + 1), 0, 0});
    for (int i = 0; i < 10; ++i) {
        GameObject& go = s.CreateGameObject("S" + std::to_string(i));
        go.transform().SetPosition({0, 0, -5.0f * (i + 1)});
        go.AddComponent<SpotLight>().range = 1.0f;
    }
    for (int i = 0; i < 3; ++i) s.CreateGameObject("Sun" + std::to_string(i)).AddComponent<DirectionalLight>();

    const LightCollection lc = CollectLights(s, glm::vec3(0.0f));
    ASSERT_EQ(lc.points.size(), static_cast<std::size_t>(kMaxPointLights));
    EXPECT_EQ(lc.spots.size(), static_cast<std::size_t>(kMaxSpotLights));
    EXPECT_EQ(lc.directional.size(), static_cast<std::size_t>(kMaxDirectionalLights));
    EXPECT_EQ(lc.droppedPoint, 4);
    EXPECT_EQ(lc.droppedSpot, 2);
    EXPECT_EQ(lc.droppedDirectional, 2);
    // Najblizsze najpierw: P0..P7.
    for (int i = 0; i < kMaxPointLights; ++i) EXPECT_EQ(lc.points[i]->gameObject().GetName(), "P" + std::to_string(i));
    EXPECT_EQ(lc.spots.back()->gameObject().GetName(), "S7");
}

TEST(LightCollection, UserLimitsAreClampedToHardLimits) {
    Scene s;
    for (int i = 0; i < 20; ++i) AddPoint(s, "P" + std::to_string(i), {float(i), 0, 0});
    LightLimits lim;
    lim.maxPoint = 100;
    EXPECT_EQ(CollectLights(s, glm::vec3(0), lim).points.size(), static_cast<std::size_t>(kMaxPointLights));
    lim.maxPoint = 3;
    EXPECT_EQ(CollectLights(s, glm::vec3(0), lim).points.size(), 3u);
    lim.maxPoint = -1;
    EXPECT_TRUE(CollectLights(s, glm::vec3(0), lim).points.empty());
}

TEST(LightCollection, CameraInsideRangeWinsAndTiesPreferBrighter) {
    Scene s;
    AddPoint(s, "FarBig", {50, 0, 0}, 60.0f, 1.0f);    // kamera w zasiegu -> priorytet 0
    AddPoint(s, "NearSmall", {3, 0, 0}, 1.0f, 1.0f);   // 2 jednostki do brzegu
    AddPoint(s, "Dim", {0.5f, 0, 0}, 5.0f, 0.5f);      // tez w zasiegu, ale ciemniejsze
    LightLimits lim;
    lim.maxPoint = 2;
    const LightCollection lc = CollectLights(s, glm::vec3(0.0f), lim);
    ASSERT_EQ(lc.points.size(), 2u);
    EXPECT_EQ(lc.points[0]->gameObject().GetName(), "FarBig");
    EXPECT_EQ(lc.points[1]->gameObject().GetName(), "Dim");
}

TEST(LightCollection, BrightestDirectionalWins) {
    Scene s;
    auto& a = s.CreateGameObject("A").AddComponent<DirectionalLight>();
    auto& b = s.CreateGameObject("B").AddComponent<DirectionalLight>();
    a.intensity = 0.5f;
    b.intensity = 2.0f;
    const LightCollection lc = CollectLights(s, glm::vec3(0.0f));
    ASSERT_EQ(lc.directional.size(), 1u);
    EXPECT_EQ(lc.directional[0], &b);
}

TEST(LightCollection, PackLightsFillsStd140Block) {
    Scene s;
    GameObject& sun = s.CreateGameObject("Sun");
    sun.transform().SetLocalEulerAngles({-90.0f, 0.0f, 0.0f}); // -Z obrocone w dol
    auto& d = sun.AddComponent<DirectionalLight>();
    d.color = {1.0f, 0.5f, 0.25f};
    d.intensity = 2.0f;
    AddPoint(s, "P", {1, 2, 3}, 7.0f, 3.0f);
    GameObject& spotGo = s.CreateGameObject("Spot");
    spotGo.transform().SetPosition({0, 5, 0});
    spotGo.transform().LookAt({0, 0, 0});
    auto& sp = spotGo.AddComponent<SpotLight>();
    sp.innerAngle = 10.0f;
    sp.outerAngle = 25.0f;

    const GpuLightBlock b = PackLights(CollectLights(s, glm::vec3(0.0f)), glm::vec3(0.1f));
    EXPECT_EQ(b.counts, glm::ivec4(1, 1, 1, 0));
    ExpectVecNear(glm::vec3(b.ambientColor), glm::vec3(0.1f));
    ExpectVecNear(glm::vec3(b.directionalDirection), glm::vec3(0, -1, 0));
    ExpectVecNear(glm::vec3(b.directionalColor), glm::vec3(2.0f, 1.0f, 0.5f));
    ExpectVecNear(glm::vec3(b.points[0].positionRange), glm::vec3(1, 2, 3));
    EXPECT_FLOAT_EQ(b.points[0].positionRange.w, 7.0f);
    ExpectVecNear(glm::vec3(b.points[0].colorIntensity), glm::vec3(3.0f));
    ExpectVecNear(glm::vec3(b.spots[0].directionCosOuter), glm::vec3(0, -1, 0));
    EXPECT_NEAR(b.spots[0].directionCosOuter.w, std::cos(glm::radians(25.0f)), 1e-6f);
    EXPECT_NEAR(b.spots[0].cosInner.x, std::cos(glm::radians(10.0f)), 1e-6f);
    // Nieuzywane wpisy sa wyzerowane.
    EXPECT_EQ(b.points[1].colorIntensity, glm::vec4(0.0f));
}

TEST(LightCollection, EmptySceneGivesEmptyBlock) {
    Scene s;
    const GpuLightBlock b = PackLights(CollectLights(s, glm::vec3(0.0f)), glm::vec3(0.0f));
    EXPECT_EQ(b.counts, glm::ivec4(0));
}
