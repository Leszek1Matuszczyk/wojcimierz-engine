// ============================================================
//  Wojcimierz Engine: audio/detail/Finite.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
// TYMCZASOWE: po rebase na #4 zastapione przez wz::IsFinite / wz::SanitizeFloat
// z engine/core/Math (ta sama semantyka: nie-skonczona -> fallback).
#include <cmath>
#include <glm/glm.hpp>

namespace wz::audio_detail {
inline bool IsFinite(float v) { return std::isfinite(v); }
inline bool IsFinite(const glm::vec3& v) { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z); }
inline float SanitizeFloat(float v, float fallback) { return IsFinite(v) ? v : fallback; }
} // namespace wz::audio_detail
