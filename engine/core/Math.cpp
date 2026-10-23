// =============================================================================
// Wojcimierz Engine - engine/core/Math.cpp
// Na chwałę czcigodnego Wojcimierza, którego mądrość
// prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// =============================================================================
#include "core/Math.h"

#include <bit>
#include <cstdint>
#include <cstdio>

namespace wz {

bool IsFinite(float v) noexcept {
    // Wykladnik z samymi jedynkami = Inf albo NaN (IEEE 754 binary32).
    const auto bits = std::bit_cast<std::uint32_t>(v);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

bool IsFinite(double v) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(v);
    return (bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}

bool IsFinite(const glm::vec3& v) noexcept {
    return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z);
}

bool IsFinite(const glm::quat& q) noexcept {
    return IsFinite(q.x) && IsFinite(q.y) && IsFinite(q.z) && IsFinite(q.w);
}

namespace {
// Silnik nie ma jeszcze wspolnego loggera - ostrzegamy na stderr.
// Gdy logger powstanie, wystarczy podmienic te jedna funkcje (Wojcimierz
// cierpliwie czeka na porzadny logger, jak na wszystko, co dobre).
void WarnNonFinite(const char* what, const char* kind) {
    std::fprintf(stderr, "[wz] OSTRZEZENIE: nieskonczona wartosc (NaN/Inf) %s w '%s' - uzyto wartosci zastepczej\n",
                 kind, what ? what : "(nieznane)");
}
}  // namespace

float SanitizeFloat(float value, float fallback, const char* what) {
    if (IsFinite(value)) return value;
    WarnNonFinite(what, "float");
    return fallback;
}

double SanitizeDouble(double value, double fallback, const char* what) {
    if (IsFinite(value)) return value;
    WarnNonFinite(what, "double");
    return fallback;
}

glm::vec3 SanitizeVec3(const glm::vec3& value, const glm::vec3& fallback, const char* what) {
    if (IsFinite(value)) return value;
    WarnNonFinite(what, "vec3");
    return fallback;
}

glm::quat SanitizeQuat(const glm::quat& value, const glm::quat& fallback, const char* what) {
    if (IsFinite(value)) return value;
    WarnNonFinite(what, "quat");
    return fallback;
}

}  // namespace wz
