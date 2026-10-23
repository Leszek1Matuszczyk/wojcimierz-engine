// ============================================================
//  Wojcimierz Engine: render/Light.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Light.h"

#include <algorithm>
#include <cmath>

namespace wz {

bool Light::IsContributing() const {
    return intensity > 0.0f && (color.r > 0.0f || color.g > 0.0f || color.b > 0.0f);
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
    const float outer = std::clamp(outerAngleDeg, 0.1f, 89.0f);
    const float inner = std::clamp(innerAngleDeg, 0.0f, outer);
    const float toRad = 3.14159265358979f / 180.0f;
    return {std::cos(inner * toRad), std::cos(outer * toRad)};
}

} // namespace lighting
} // namespace wz
