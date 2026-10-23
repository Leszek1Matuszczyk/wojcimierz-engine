// ============================================================
//  Wojcimierz Engine: tests/test_shadows.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "render/Camera.h"
#include "render/Light.h"
#include "render/Shadows.h"
#include "scene/Scene.h"

#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

using namespace wz;
using namespace wz::shadows;

namespace {

glm::vec3 ToNdc(const glm::mat4& m, glm::vec3 p) {
    const glm::vec4 c = m * glm::vec4(p, 1.0f);
    return glm::vec3(c) / c.w;
}

bool InsideNdc(glm::vec3 n, float eps = 1e-4f) {
    return std::abs(n.x) <= 1.0f + eps && std::abs(n.y) <= 1.0f + eps && std::abs(n.z) <= 1.0f + eps;
}

Camera& MakeCam(Scene& s, glm::vec3 pos, glm::vec3 target) {
    GameObject& go = s.CreateGameObject("Cam");
    auto& cam = go.AddComponent<Camera>();
    cam.nearClipPlane = 0.1f;
    cam.farClipPlane = 100.0f;
    go.transform().SetPosition(pos);
    go.transform().LookAt(target);
    return cam;
}

const glm::vec3 kSun = glm::normalize(glm::vec3(0.4f, -1.0f, -0.3f));

} // namespace

TEST(Shadows, FrustumCornersOfIdentityAreNdcCube) {
    const FrustumCorners c = ComputeFrustumCorners(glm::mat4(1.0f));
    ExpectVecNear(c[0], {-1, -1, -1});
    ExpectVecNear(c[7], {1, 1, 1});
}

TEST(Shadows, CameraSliceIsClampedToShadowDistance) {
    Scene s;
    Camera& cam = MakeCam(s, {0, 0, 0}, {0, 0, -1});
    const FrustumCorners near20 = CameraFrustumSlice(cam, 1.0f, 20.0f);
    // Kamera patrzy w -Z: blizsza plaszczyzna z = -0.1, dalsza z = -20.
    for (int i = 0; i < 4; ++i) EXPECT_NEAR(near20[i].z, -0.1f, 1e-3f);
    for (int i = 4; i < 8; ++i) EXPECT_NEAR(near20[i].z, -20.0f, 1e-2f);
    // shadowDistance > far -> uzywamy far.
    const FrustumCorners full = CameraFrustumSlice(cam, 1.0f, 1000.0f);
    for (int i = 4; i < 8; ++i) EXPECT_NEAR(full[i].z, -100.0f, 0.1f);
}

TEST(Shadows, DirectionalFitContainsSliceAndCasterMargin) {
    Scene s;
    Camera& cam = MakeCam(s, {3, 4, 8}, {0, 0, 0});
    const FrustumCorners c = CameraFrustumSlice(cam, 16.0f / 9.0f, 25.0f);
    const float margin = 30.0f;
    const LightSpace ls = FitDirectionalShadow(kSun, c, 2048, margin);
    for (const auto& p : c) {
        EXPECT_TRUE(InsideNdc(ToNdc(ls.viewProjection, p))) << p.x << "," << p.y << "," << p.z;
        // Rzucajacy cien do `margin` przed wycinkiem (od strony swiatla) tez jest w mapie.
        EXPECT_TRUE(InsideNdc(ToNdc(ls.viewProjection, p - kSun * (margin * 0.99f))));
    }
    // Glebia rosnie wzdluz kierunku swiecenia (blizej swiatla = mniejsza glebia).
    const glm::vec3 a = ToNdc(ls.viewProjection, {0, 0, 0});
    const glm::vec3 b = ToNdc(ls.viewProjection, kSun * 1.0f);
    EXPECT_GT(b.z, a.z);
    EXPECT_NEAR(a.x, b.x, 1e-4f); // przesuniecie wzdluz swiatla nie zmienia xy
    EXPECT_NEAR(a.y, b.y, 1e-4f);
    EXPECT_GT(ls.texelWorldSize, 0.0f);
}

TEST(Shadows, DirectionalFitIsStableUnderCameraRotation) {
    // Rozmiar mapy (teksel) nie zalezy od obrotu kamery - inaczej cienie "pulsowalyby".
    Scene s;
    Camera& cam = MakeCam(s, {0, 2, 0}, {0, 2, -1});
    const float t0 = FitDirectionalShadow(kSun, CameraFrustumSlice(cam, 1.5f, 30.0f), 1024, 10.0f).texelWorldSize;
    for (float yaw : {17.0f, 45.0f, 133.0f, 270.0f}) {
        cam.transform().SetLocalEulerAngles({0.0f, yaw, 0.0f});
        const float t = FitDirectionalShadow(kSun, CameraFrustumSlice(cam, 1.5f, 30.0f), 1024, 10.0f).texelWorldSize;
        EXPECT_NEAR(t, t0, 1e-5f) << "yaw " << yaw;
    }
}

TEST(Shadows, DirectionalFitSnapsToTexelGrid) {
    // Przesuniecie kamery o ulamek teksela przesuwa mape o calkowita liczbe tekseli (0 albo 1),
    // wiec punkty swiata trafiaja zawsze w te same miejsca teksela - brak migotania krawedzi.
    Scene s;
    Camera& cam = MakeCam(s, {0, 3, 6}, {0, 0, 0});
    const int size = 512;
    const LightSpace a = FitDirectionalShadow(kSun, CameraFrustumSlice(cam, 1.0f, 20.0f), size, 5.0f);
    cam.transform().SetPosition({0.37f * a.texelWorldSize, 3, 6});
    const LightSpace b = FitDirectionalShadow(kSun, CameraFrustumSlice(cam, 1.0f, 20.0f), size, 5.0f);
    ASSERT_NEAR(a.texelWorldSize, b.texelWorldSize, 1e-6f);
    const glm::vec3 p(1.234f, 0.5f, -2.0f);
    const float dx = (ToNdc(b.viewProjection, p).x - ToNdc(a.viewProjection, p).x) * 0.5f * size;
    const float dy = (ToNdc(b.viewProjection, p).y - ToNdc(a.viewProjection, p).y) * 0.5f * size;
    EXPECT_NEAR(dx, std::round(dx), 2e-3f);
    EXPECT_NEAR(dy, std::round(dy), 2e-3f);
}

TEST(Shadows, StraightDownLightHasFiniteMatrices) {
    Scene s;
    Camera& cam = MakeCam(s, {0, 5, 5}, {0, 0, 0});
    const LightSpace ls = FitDirectionalShadow({0, -1, 0}, CameraFrustumSlice(cam, 1.0f, 10.0f), 1024, 5.0f);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) EXPECT_TRUE(std::isfinite(ls.viewProjection[i][j]));
    EXPECT_TRUE(InsideNdc(ToNdc(ls.viewProjection, {0, 0, 0})));
    const LightSpace sp = SpotShadow({0, 4, 0}, {0, -1, 0}, 30.0f, 10.0f, 512);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) EXPECT_TRUE(std::isfinite(sp.viewProjection[i][j]));
}

TEST(Shadows, SpotShadowCoversConeAndRange) {
    const glm::vec3 pos(1, 5, 2), dir = glm::normalize(glm::vec3(0.2f, -1.0f, 0.1f));
    const float outer = 35.0f, range = 12.0f;
    const LightSpace ls = SpotShadow(pos, dir, outer, range, 1024);
    // Os stozka -> srodek mapy.
    const glm::vec3 axis = ToNdc(ls.viewProjection, pos + dir * 6.0f);
    EXPECT_NEAR(axis.x, 0.0f, 1e-4f);
    EXPECT_NEAR(axis.y, 0.0f, 1e-4f);
    // Punkt na krawedzi stozka (outer) w zasiegu - w mapie.
    const glm::vec3 side = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0)));
    const glm::vec3 edgeDir = glm::normalize(dir * std::cos(glm::radians(outer)) + side * std::sin(glm::radians(outer)));
    EXPECT_TRUE(InsideNdc(ToNdc(ls.viewProjection, pos + edgeDir * 8.0f)));
    // Glebia rosnie z odlegloscia, poza zasiegiem > 1.
    EXPECT_LT(ToNdc(ls.viewProjection, pos + dir * 2.0f).z, ToNdc(ls.viewProjection, pos + dir * 8.0f).z);
    EXPECT_GT(ToNdc(ls.viewProjection, pos + dir * (range * 1.1f)).z, 1.0f);
    // texelWorldSize ~ szerokosc mapy w odleglosci range/2 / rozdzielczosc.
    const float expected = 2.0f * 6.0f * std::tan(glm::radians(outer + 2.0f)) / 1024.0f;
    EXPECT_NEAR(ls.texelWorldSize, expected, expected * 1e-3f);
}

TEST(Shadows, ShadowParametersAreSanitized) {
    // Straz NaN/Inf (render/detail/FloatGuard.h): bledne pola cieni -> wartosci domyslne.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    Scene s;
    auto& sun = s.CreateGameObject("Sun").AddComponent<DirectionalLight>();
    sun.shadowStrength = nan;
    sun.shadowBias = inf;
    sun.normalBias = -inf;
    EXPECT_FLOAT_EQ(sun.GetShaderShadowStrength(), Light::kDefaultShadowStrength);
    EXPECT_FLOAT_EQ(sun.GetShaderShadowBias(), Light::kDefaultShadowBias);
    EXPECT_FLOAT_EQ(sun.GetShaderNormalBias(), Light::kDefaultNormalBias);
    auto& spot = s.CreateGameObject("Spot").AddComponent<SpotLight>();
    spot.shadowStrength = 3.0f; // skonczone, ale poza zakresem -> obciete
    spot.shadowBias = -1.0f;
    EXPECT_FLOAT_EQ(spot.GetShaderShadowStrength(), 1.0f);
    EXPECT_FLOAT_EQ(spot.GetShaderShadowBias(), 0.0f);
}

TEST(Shadows, ShadowParametersKeepLastGoodValue) {
    // Ostatnia dobra wartosc zamiast domyslnej: cien nie "mruga" przy chwilowym NaN.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    Scene s;
    auto& spot = s.CreateGameObject("Spot").AddComponent<SpotLight>();
    spot.shadowStrength = 0.4f;
    spot.shadowBias = 0.2f;
    spot.normalBias = 1.5f;
    EXPECT_FLOAT_EQ(spot.GetShaderShadowStrength(), 0.4f);
    EXPECT_FLOAT_EQ(spot.GetShaderShadowBias(), 0.2f);
    EXPECT_FLOAT_EQ(spot.GetShaderNormalBias(), 1.5f);
    spot.shadowStrength = nan;
    spot.shadowBias = -inf;
    spot.normalBias = inf;
    EXPECT_FLOAT_EQ(spot.GetShaderShadowStrength(), 0.4f);
    EXPECT_FLOAT_EQ(spot.GetShaderShadowBias(), 0.2f);
    EXPECT_FLOAT_EQ(spot.GetShaderNormalBias(), 1.5f);
    // Ostatnia dobra, ale poza zakresem -> i tak obcieta (sanityzacja, potem clamp).
    spot.shadowStrength = 7.0f;
    EXPECT_FLOAT_EQ(spot.GetShaderShadowStrength(), 1.0f);
    spot.shadowStrength = nan;
    EXPECT_FLOAT_EQ(spot.GetShaderShadowStrength(), 1.0f);
}

TEST(Shadows, HugeFiniteShadowValuesAreClampedAndMatricesStayFinite) {
    // Jak fizyka w #22: FLT_MAX jest skonczone, ale macierz ortho/perspektywy z nim - juz nie.
    const float huge = std::numeric_limits<float>::max();
    render_detail::ResetNonFiniteWarnings();
    Scene s;
    auto& spot = s.CreateGameObject("HugeSpot").AddComponent<SpotLight>();
    spot.shadowStrength = huge;
    spot.shadowBias = huge;
    spot.normalBias = -huge;
    spot.range = huge;
    spot.outerAngle = huge;
    EXPECT_FLOAT_EQ(spot.GetShaderShadowStrength(), 1.0f);
    EXPECT_FLOAT_EQ(spot.GetShaderShadowBias(), Light::kMaxShadowBias);
    EXPECT_FLOAT_EQ(spot.GetShaderNormalBias(), 0.0f);
    EXPECT_EQ(render_detail::OutOfRangeWarningCount(), 1); // raz na obiekt

    const shadows::LightSpace sl = shadows::SpotShadow({0, 0, 0}, {0, -1, 0}, spot.GetShaderOuterAngle(),
                                                       spot.GetShaderRange(), 1024);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) EXPECT_TRUE(std::isfinite(sl.viewProjection[c][r]));
    EXPECT_TRUE(std::isfinite(sl.texelWorldSize));

    int owner = 0;
    shadows::ShadowSettingsGuard g;
    EXPECT_FLOAT_EQ(g.Distance(30.0f, 40.0f, &owner), 30.0f);
    EXPECT_FLOAT_EQ(g.Distance(std::numeric_limits<float>::quiet_NaN(), 40.0f, &owner), 30.0f); // ostatnia dobra
    EXPECT_FLOAT_EQ(g.Distance(huge, 40.0f, &owner), shadows::kMaxShadowDistance);
    // Ostatnia dobra = ostatnia SKONCZONA (tu FLT_MAX), wiec po NaN i tak wraca przycieta.
    EXPECT_FLOAT_EQ(g.Distance(std::numeric_limits<float>::quiet_NaN(), 40.0f, &owner), shadows::kMaxShadowDistance);
    EXPECT_FLOAT_EQ(g.Distance(-5.0f, 40.0f, &owner), shadows::kMinShadowDistance);
    EXPECT_FLOAT_EQ(g.CasterMargin(huge, 50.0f, &owner), shadows::kMaxCasterMargin);
    EXPECT_FLOAT_EQ(g.CasterMargin(-huge, 50.0f, &owner), 0.0f);
    EXPECT_EQ(render_detail::OutOfRangeWarningCount(), 2); // drugi wlasciciel

    Camera& cam = MakeCam(s, {0, 2, 5}, {0, 0, 0});
    cam.farClipPlane = 1e6f; // zeby to shadowDistance (1e5) ograniczalo wycinek
    const shadows::FrustumCorners slice =
        shadows::CameraFrustumSlice(cam, 16.0f / 9.0f, g.Distance(huge, 40.0f, &owner));
    const shadows::LightSpace dl =
        shadows::FitDirectionalShadow(glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f)), slice, 2048,
                                      g.CasterMargin(huge, 50.0f, &owner));
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) EXPECT_TRUE(std::isfinite(dl.viewProjection[c][r]));
    EXPECT_TRUE(std::isfinite(dl.texelWorldSize));
    render_detail::ResetNonFiniteWarnings();
}
