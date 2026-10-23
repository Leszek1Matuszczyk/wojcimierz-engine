// ============================================================
//  Wojcimierz Engine: tests/nonfinite/NonFiniteUtils.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include <gtest/gtest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <string>
#include <vector>

namespace wztest {

// Wartosci, ktore kazdy publiczny setter musi odrzucic (albo zastapic fallbackiem):
// po wywolaniu stan obiektu ma byc skonczony - patrz #1, #16, B1 z #9 i wz::SanitizeFloat z #4.
inline constexpr float kBadValues[] = {
    std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity(),
};

inline std::string BadName(float v) {
    if (std::isnan(v)) return "NaN";
    return v > 0 ? "PlusInf" : "MinusInf";
}

inline bool Finite(float v) { return std::isfinite(v); }
inline bool Finite(const glm::vec3& v) { return Finite(v.x) && Finite(v.y) && Finite(v.z); }
inline bool Finite(const glm::vec4& v) { return Finite(v.x) && Finite(v.y) && Finite(v.z) && Finite(v.w); }
inline bool Finite(const glm::quat& q) { return Finite(q.x) && Finite(q.y) && Finite(q.z) && Finite(q.w); }
inline bool Finite(const glm::mat4& m) {
    for (int c = 0; c < 4; ++c)
        if (!Finite(m[c])) return false;
    return true;
}

// Wektor z jedna zla skladowa na osi 'axis' (0..2) - reszta poprawna.
inline glm::vec3 PoisonVec(float bad, int axis, glm::vec3 base = {1.0f, 2.0f, 3.0f}) {
    base[axis] = bad;
    return base;
}

inline glm::quat PoisonQuat(float bad, int comp) {
    glm::quat q{1.0f, 0.0f, 0.0f, 0.0f};
    q[comp] = bad;
    return q;
}

struct BadCase {
    float value;
    int axis;
};

inline std::string CaseName(const ::testing::TestParamInfo<BadCase>& info) {
    return BadName(info.param.value) + "_axis" + std::to_string(info.param.axis);
}

inline auto AllBadCases(int axes = 3) {
    std::vector<BadCase> cases;
    for (float v : kBadValues)
        for (int a = 0; a < axes; ++a) cases.push_back({v, a});
    return ::testing::ValuesIn(cases);
}

} // namespace wztest

namespace wztest {
inline void PrintTo(const BadCase& c, std::ostream* os) { *os << BadName(c.value) << " na osi " << c.axis; }
} // namespace wztest
