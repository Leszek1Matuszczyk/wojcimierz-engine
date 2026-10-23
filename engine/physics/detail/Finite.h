// ============================================================
//  Wojcimierz Engine: physics/detail/Finite.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
// TYMCZASOWE: po rebase na #4 zastapione przez wz::IsFinite / wz::SanitizeFloat
// z engine/core/Math.h (ta sama semantyka: nie-skonczona -> fallback).
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace wz::physics_detail {
inline bool IsFinite(float v) { return std::isfinite(v); }
inline bool IsFinite(const glm::vec3& v) { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z); }
inline bool IsFinite(const glm::quat& q) { return IsFinite(q.x) && IsFinite(q.y) && IsFinite(q.z) && IsFinite(q.w); }
inline float SanitizeFloat(float v, float fallback) { return IsFinite(v) ? v : fallback; }
inline glm::vec3 SanitizeVec3(const glm::vec3& v, const glm::vec3& fallback) { return IsFinite(v) ? v : fallback; }
} // namespace wz::physics_detail
