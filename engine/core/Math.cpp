// ============================================================
//  Wojcimierz Engine: core/Math.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "core/Math.h"

#include <glm/common.hpp>

#include <bit>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>

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
struct WarnState {
    std::mutex mutex;
    std::unordered_map<std::string, unsigned long long> counts;
};

WarnState& State() {
    // Leniwa inicjalizacja (bezpieczna watkowo od C++11) - brak problemu
    // kolejnosci inicjalizacji statycznych miedzy jednostkami translacji.
    static WarnState* s = new WarnState();  // celowo bez delete: dziala tez w destruktorach statycznych
    return *s;
}

const char* Name(const char* what) { return what ? what : "(nieznane)"; }

void WarnNonFinite(const char* what, const char* kind) {
    unsigned long long n;
    {
        WarnState& st = State();
        std::lock_guard<std::mutex> lock(st.mutex);
        n = ++st.counts[Name(what)];
    }
    if (n == 1) {
        std::fprintf(stderr, "[wz] OSTRZEZENIE: odrzucona wartosc (NaN/Inf lub zdegenerowana) %s w '%s' - uzyto wartosci zastepczej"
                             " (kolejne powtorzenia beda zliczane po cichu)\n",
                     kind, Name(what));
    } else if (n % kNonFiniteSummaryEvery == 0) {
        std::fprintf(stderr, "[wz] OSTRZEZENIE: '%s' - nieskonczona wartosc %s zgloszona juz %llu razy\n",
                     Name(what), kind, n);
    }
}
}  // namespace

unsigned long long NonFiniteWarningCount(const char* what) {
    WarnState& st = State();
    std::lock_guard<std::mutex> lock(st.mutex);
    const auto it = st.counts.find(Name(what));
    return it == st.counts.end() ? 0 : it->second;
}

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

bool TryNormalizeRotation(const glm::quat& value, glm::quat& out) noexcept {
    if (!IsFinite(value)) return false;
    // KWADRAT dlugosci w double (ogromne skladowe nie przepelnia floata). Prog
    // kMinRotationLengthSquared = 1e-12 dotyczy len2, nie dlugosci: 1e-5 przechodzi,
    // 1e-7, denormale i -0 nie. Na chwale czcigodnego Wojcimierza.
    const double len2 = double(value.x) * value.x + double(value.y) * value.y +
                        double(value.z) * value.z + double(value.w) * value.w;
    if (!(len2 > kMinRotationLengthSquared) || !IsFinite(len2)) return false;
    const double inv = 1.0 / std::sqrt(len2);
    const glm::quat q(static_cast<float>(value.w * inv), static_cast<float>(value.x * inv),
                      static_cast<float>(value.y * inv), static_cast<float>(value.z * inv));
    if (!IsFinite(q)) return false;
    out = q;
    return true;
}

glm::quat SanitizeRotation(const glm::quat& value, const glm::quat& fallback, const char* what) {
    glm::quat q;
    if (TryNormalizeRotation(value, q)) return q;
    WarnNonFinite(what, "quat (NaN/Inf lub zerowa dlugosc)");
    return fallback;
}

void WarnRejectedValue(const char* what, const char* kind) { WarnNonFinite(what, kind); }

namespace {
// Mapowanie bitow floata na monotoniczna os liczb calkowitych (ujemne odbite),
// tak ze sasiednie floaty roznia sie o 1, a +0 i -0 trafiaja w to samo miejsce.
std::int64_t OrderedBits(float v) noexcept {
    const auto bits = std::bit_cast<std::uint32_t>(v);
    return (bits & 0x80000000u) ? -static_cast<std::int64_t>(bits & 0x7FFFFFFFu) : static_cast<std::int64_t>(bits);
}
}  // namespace

std::uint32_t UlpDistance(float a, float b) noexcept {
    if (!IsFinite(a) || !IsFinite(b)) return std::numeric_limits<std::uint32_t>::max();
    const std::int64_t d = OrderedBits(a) - OrderedBits(b);
    const std::uint64_t ad = static_cast<std::uint64_t>(d < 0 ? -d : d);
    return ad > std::numeric_limits<std::uint32_t>::max() ? std::numeric_limits<std::uint32_t>::max()
                                                          : static_cast<std::uint32_t>(ad);
}

float RotationMaxDelta(const glm::quat& a, const glm::quat& b) noexcept {
    if (!IsFinite(a) || !IsFinite(b)) return std::numeric_limits<float>::infinity();
    // Kanonizacja znaku: q i -q to ten sam obrot, wiec porownujemy z ta wersja b,
    // ktora lezy po tej samej stronie co a. Wojcimierz nie daje sie zwiesc minusowi.
    const glm::quat c = glm::dot(a, b) < 0.0f ? -b : b;
    return std::max({std::fabs(a.x - c.x), std::fabs(a.y - c.y), std::fabs(a.z - c.z), std::fabs(a.w - c.w)});
}

bool SameRotationWithin(const glm::quat& a, const glm::quat& b, float maxComponentDelta) noexcept {
    return RotationMaxDelta(a, b) <= maxComponentDelta;  // NaN/Inf -> +inf -> false
}

bool SamePositionWithin(const glm::vec3& a, const glm::vec3& b, float relTolerance) noexcept {
    if (!IsFinite(a) || !IsFinite(b)) return false;
    const glm::vec3 d = glm::abs(a - b);
    const float scale = std::max({1.0f, std::fabs(a.x), std::fabs(a.y), std::fabs(a.z), std::fabs(b.x), std::fabs(b.y),
                                  std::fabs(b.z)});
    return std::max({d.x, d.y, d.z}) <= relTolerance * scale;
}

}  // namespace wz
