// ============================================================
//  Wojcimierz Engine: render/detail/FloatGuard.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/detail/FloatGuard.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_set>

namespace wz::render_detail {

// --- Czesc 1: kopia wz:: z #4 (56c7658). TODO(#4, 56c7658): usunac po rebase. ---

bool IsFinite(float v) noexcept {
    // Wykladnik z samymi jedynkami = Inf albo NaN; dziala tez przy -ffast-math.
    const auto bits = std::bit_cast<std::uint32_t>(v);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

bool IsFinite(double v) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(v);
    return (bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}

bool IsFinite(const glm::vec3& v) noexcept { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z); }

bool IsFinite(const glm::quat& q) noexcept { return IsFinite(q.x) && IsFinite(q.y) && IsFinite(q.z) && IsFinite(q.w); }

namespace {
void WarnNonFinite(const char* what, const char* kind) {
    std::fprintf(stderr, "[wz] OSTRZEZENIE: nieskonczona wartosc (NaN/Inf) %s w '%s' - uzyto wartosci zastepczej\n",
                 kind, what ? what : "(nieznane)");
}
} // namespace

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

// --- Czesc 2: ostrzezenie raz na obiekt ---

namespace {
std::mutex g_mutex;
std::unordered_set<const void*> g_warned;
int g_count = 0;

std::string FallbackText(float fallback) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "uzyto %g", static_cast<double>(fallback));
    return buf;
}
} // namespace

void WarnNonFiniteOnce(const void* object, const std::string& objectName, const char* field, const std::string& action) {
    // Jedno ostrzezenie na obiekt - Wojcimierz upomina raz, a potem cierpliwie naprawia co klatke.
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_warned.insert(object).second) return;
    ++g_count;
    std::fprintf(stderr, "[render] '%s': %s nie jest skonczona liczba (NaN/Inf) - %s"
                         " (kolejne bledy tego obiektu nie beda zglaszane)\n",
                 objectName.c_str(), field, action.c_str());
}

int NonFiniteWarningCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_count;
}

void ResetNonFiniteWarnings() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_warned.clear();
    g_count = 0;
}

void ForgetNonFiniteWarnings(const void* object) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_warned.erase(object);
}

float SanitizeFloatOnce(float value, float fallback, const void* object, const std::string& objectName, const char* field) {
    if (IsFinite(value)) return value;
    WarnNonFiniteOnce(object, objectName, field, FallbackText(fallback));
    return fallback;
}

glm::vec3 SanitizeVec3Once(const glm::vec3& value, const glm::vec3& fallback, const void* object,
                           const std::string& objectName, const char* field) {
    if (IsFinite(value)) return value;
    WarnNonFiniteOnce(object, objectName, field, "uzyto wartosci domyslnej");
    return fallback;
}

glm::vec4 SanitizeVec4Once(const glm::vec4& value, const glm::vec4& fallback, const void* object,
                           const std::string& objectName, const char* field) {
    if (IsFinite(value)) return value;
    WarnNonFiniteOnce(object, objectName, field, "uzyto wartosci domyslnej");
    return fallback;
}

// --- Czesc 3: ostatnia dobra wartosc ---

namespace {
// Wspolna petla po kanalach. Zwraca liczbe naprawionych kanalow i opis reakcji dla ostrzezenia.
template <int N>
int KeepLastChannels(float* v, LastGood<N>& last, const float* fallback, std::string& action) {
    int fixed = 0;
    bool usedLast = false, usedDefault = false;
    for (int i = 0; i < N; ++i) {
        if (IsFinite(v[i])) {
            last.value[i] = v[i];
            last.valid[i] = true;
            continue;
        }
        ++fixed;
        if (last.valid[i]) { v[i] = last.value[i]; usedLast = true; }
        else { v[i] = fallback[i]; usedDefault = true; }
    }
    if (fixed) {
        char buf[64];
        if (N == 1) std::snprintf(buf, sizeof(buf), "%s %g", usedLast ? "zachowano ostatnia dobra" : "uzyto domyslnej",
                                  static_cast<double>(v[0]));
        else std::snprintf(buf, sizeof(buf), "%s", usedLast && usedDefault ? "zachowano ostatnie dobre / domyslne kanaly"
                                                    : usedLast ? "zachowano ostatnie dobre kanaly"
                                                               : "uzyto domyslnych kanalow");
        action = buf;
    }
    return fixed;
}
} // namespace

float SanitizeFloatKeepLast(float value, LastGoodFloat& last, float fallback, const void* object,
                            const std::string& objectName, const char* field) {
    std::string action;
    if (KeepLastChannels<1>(&value, last, &fallback, action)) WarnNonFiniteOnce(object, objectName, field, action);
    return value;
}

glm::vec3 SanitizeVec3KeepLast(const glm::vec3& value, LastGoodVec3& last, const glm::vec3& fallback,
                               const void* object, const std::string& objectName, const char* field) {
    glm::vec3 v = value;
    const float fb[3] = {fallback.x, fallback.y, fallback.z};
    std::string action;
    if (KeepLastChannels<3>(&v.x, last, fb, action)) WarnNonFiniteOnce(object, objectName, field, action);
    return v;
}

glm::vec4 SanitizeVec4KeepLast(const glm::vec4& value, LastGoodVec4& last, const glm::vec4& fallback,
                               const void* object, const std::string& objectName, const char* field) {
    glm::vec4 v = value;
    const float fb[4] = {fallback.x, fallback.y, fallback.z, fallback.w};
    std::string action;
    if (KeepLastChannels<4>(&v.x, last, fb, action)) WarnNonFiniteOnce(object, objectName, field, action);
    return v;
}

} // namespace wz::render_detail
