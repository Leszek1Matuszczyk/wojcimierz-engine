// ============================================================
//  Wojcimierz Engine: render/Light.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Light.h"
#include "render/detail/FloatGuard.h"
#include "scene/GameObject.h"
#include "scene/Transform.h"

#include <algorithm>
#include <cmath>

namespace wz {

namespace guard = render_detail;

namespace {
std::string NameOf(const Component& c) { return c.gameObject().GetName(); }
} // namespace

Light::~Light() { guard::ForgetNonFiniteWarnings(this); }

bool Light::IsContributing() const {
    const glm::vec3 c = GetShaderColor();
    return GetShaderIntensity() > 0.0f && (c.r > 0.0f || c.g > 0.0f || c.b > 0.0f);
}

float Light::GetShaderIntensity() const {
    // Najpierw NaN/Inf (ostatnia dobra), potem zakres - Wojcimierz swieci jasno, ale nie nieskonczenie.
    const std::string n = NameOf(*this);
    const float v = guard::SanitizeFloatKeepLast(intensity, lastGood_.intensity, kDefaultIntensity, this, n, "intensity");
    return guard::ClampOnce(v, 0.0f, kMaxIntensity, this, n, "intensity");
}

glm::vec3 Light::GetShaderColor() const {
    const std::string n = NameOf(*this);
    const glm::vec3 c = guard::SanitizeVec3KeepLast(color, lastGood_.color, glm::vec3(1.0f), this, n, "color");
    return guard::ClampVec3Once(c, 0.0f, kMaxColor, this, n, "color");
}

glm::vec3 Light::GetShaderRadiance() const {
    // Iloczyn w double: 1e6 * 1e6 miesci sie w float, ale liczymy bezpiecznie niezaleznie od limitow.
    const glm::dvec3 r = glm::dvec3(GetShaderColor()) * static_cast<double>(GetShaderIntensity());
    constexpr double kMax = 3.0e38;
    return glm::vec3(glm::clamp(r, glm::dvec3(0.0), glm::dvec3(kMax)));
}

bool Light::CheckFinitePosition() const {
    if (guard::IsFinite(transform().GetPosition())) return true;
    guard::WarnNonFiniteOnce(this, NameOf(*this), "pozycja (Transform)", "swiatlo pominiete");
    return false;
}

bool Light::CheckFiniteDirection() const {
    if (guard::IsFinite(transform().Forward())) return true;
    guard::WarnNonFiniteOnce(this, NameOf(*this), "kierunek (Transform)", "swiatlo pominiete");
    return false;
}

namespace {
// range == 0 to swiadomie "wylaczone" swiatlo (CollectLights je pomija) - bez ostrzezenia.
// Ujemne -> 0 z ostrzezeniem; (0, kMinLightRange) i > kMaxLightRange -> przyciete z ostrzezeniem.
float ClampLightRange(float v, const void* obj, const std::string& n) {
    if (v <= 0.0f) return guard::ClampOnce(v, 0.0f, 0.0f, obj, n, "range");
    return guard::ClampOnce(v, kMinLightRange, kMaxLightRange, obj, n, "range");
}
} // namespace

float PointLight::GetShaderRange() const {
    return ClampLightRange(guard::SanitizeFloatKeepLast(range, lastGood_.range, kDefaultRange, this, NameOf(*this), "range"),
                           this, NameOf(*this));
}

float SpotLight::GetShaderRange() const {
    return ClampLightRange(guard::SanitizeFloatKeepLast(range, lastGood_.range, kDefaultRange, this, NameOf(*this), "range"),
                           this, NameOf(*this));
}

float SpotLight::GetShaderInnerAngle() const {
    // inner <= outer: wewnetrzny stozek nie moze byc szerszy niz zewnetrzny.
    const std::string n = NameOf(*this);
    const float v = guard::SanitizeFloatKeepLast(innerAngle, lastGood_.innerAngle, kDefaultInnerAngle, this, n, "innerAngle");
    return guard::ClampOnce(v, kMinAngle, GetShaderOuterAngle(), this, n, "innerAngle");
}

float SpotLight::GetShaderOuterAngle() const {
    const std::string n = NameOf(*this);
    const float v = guard::SanitizeFloatKeepLast(outerAngle, lastGood_.outerAngle, kDefaultOuterAngle, this, n, "outerAngle");
    return guard::ClampOnce(v, kMinAngle, kMaxAngle, this, n, "outerAngle");
}

namespace lighting {

float DistanceAttenuation(float distance, float range) {
    if (!(range > 0.0f) || !(distance < range)) return 0.0f; // takze NaN
    // W double: distance^2 i (d/range)^4 nie przepelnia sie nawet dla FLT_MAX.
    const double d = distance, rg = range;
    const double d2 = std::max(d * d, 1e-4);
    const double r = d / rg;
    const double window = std::clamp(1.0 - r * r * r * r, 0.0, 1.0);
    return static_cast<float>(window * window / d2);
}

float SpotAttenuation(float cosAngle, float cosInner, float cosOuter) {
    const float denom = std::max(cosInner - cosOuter, 1e-4f);
    const float t = std::clamp((cosAngle - cosOuter) / denom, 0.0f, 1.0f);
    return t * t;
}

SpotCone ComputeSpotCone(float innerAngleDeg, float outerAngleDeg) {
    // Stozek musi byc wezszy niz polsfera; inner nie moze przekroczyc outer.
    // NaN/Inf -> domyslne katy spota (straz na wypadek wywolania z pominieciem GetShader*).
    if (!render_detail::IsFinite(outerAngleDeg)) outerAngleDeg = SpotLight::kDefaultOuterAngle;
    if (!render_detail::IsFinite(innerAngleDeg)) innerAngleDeg = SpotLight::kDefaultInnerAngle;
    const float outer = std::clamp(outerAngleDeg, SpotLight::kMinAngle, SpotLight::kMaxAngle);
    const float inner = std::clamp(innerAngleDeg, SpotLight::kMinAngle, outer);
    const float toRad = 3.14159265358979f / 180.0f;
    return {std::cos(inner * toRad), std::cos(outer * toRad)};
}

} // namespace lighting
} // namespace wz
