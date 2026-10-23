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
    return guard::SanitizeFloatOnce(intensity, kDefaultIntensity, this, NameOf(*this), "intensity");
}

glm::vec3 Light::GetShaderColor() const {
    return guard::SanitizeVec3Once(color, glm::vec3(1.0f), this, NameOf(*this), "color");
}

float Light::GetShaderShadowStrength() const {
    return std::clamp(guard::SanitizeFloatOnce(shadowStrength, kDefaultShadowStrength, this, NameOf(*this), "shadowStrength"),
                      0.0f, 1.0f);
}

float Light::GetShaderShadowBias() const {
    return std::max(guard::SanitizeFloatOnce(shadowBias, kDefaultShadowBias, this, NameOf(*this), "shadowBias"), 0.0f);
}

float Light::GetShaderNormalBias() const {
    return std::max(guard::SanitizeFloatOnce(normalBias, kDefaultNormalBias, this, NameOf(*this), "normalBias"), 0.0f);
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

float PointLight::GetShaderRange() const {
    return guard::SanitizeFloatOnce(range, kDefaultRange, this, NameOf(*this), "range");
}

float SpotLight::GetShaderRange() const {
    return guard::SanitizeFloatOnce(range, kDefaultRange, this, NameOf(*this), "range");
}

float SpotLight::GetShaderInnerAngle() const {
    return guard::SanitizeFloatOnce(innerAngle, kDefaultInnerAngle, this, NameOf(*this), "innerAngle");
}

float SpotLight::GetShaderOuterAngle() const {
    return guard::SanitizeFloatOnce(outerAngle, kDefaultOuterAngle, this, NameOf(*this), "outerAngle");
}

namespace lighting {

float DistanceAttenuation(float distance, float range) {
    if (range <= 0.0f || distance >= range) return 0.0f;
    const float d2 = std::max(distance * distance, 1e-4f);
    const float r = distance / range;
    const float window = std::clamp(1.0f - r * r * r * r, 0.0f, 1.0f);
    return window * window / d2;
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
    const float outer = std::clamp(outerAngleDeg, 0.1f, 89.0f);
    const float inner = std::clamp(innerAngleDeg, 0.0f, outer);
    const float toRad = 3.14159265358979f / 180.0f;
    return {std::cos(inner * toRad), std::cos(outer * toRad)};
}

} // namespace lighting
} // namespace wz
