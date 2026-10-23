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

// --- Czesc 1: wz::IsFinite/Sanitize* z core/Math.h (#4) - brak lokalnej kopii. ---

// --- Czesc 2: ostrzezenie raz na obiekt ---

namespace {
std::mutex g_mutex;
std::unordered_set<const void*> g_warned;
int g_count = 0;
std::unordered_set<const void*> g_rangeWarned; // osobny rejestr: NaN nie wycisza ostrzezenia o zakresie
int g_rangeCount = 0;

bool AlreadyWarned(const void* object) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_warned.count(object) != 0;
}

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
    g_rangeWarned.clear();
    g_rangeCount = 0;
}

void ForgetNonFiniteWarnings(const void* object) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_warned.erase(object);
    g_rangeWarned.erase(object);
}

float SanitizeFloatOnce(float value, float fallback, const void* object, const std::string& objectName, const char* field) {
    if (IsFinite(value)) return value;
    // Pierwszy blad obiektu: wz::SanitizeFloat (#4) z jego ostrzezeniem; kolejne - cicho.
    const std::string what = objectName + "." + field;
    const float v = AlreadyWarned(object) ? fallback : wz::SanitizeFloat(value, fallback, what.c_str());
    WarnNonFiniteOnce(object, objectName, field, FallbackText(fallback));
    return v;
}

glm::vec3 SanitizeVec3Once(const glm::vec3& value, const glm::vec3& fallback, const void* object,
                           const std::string& objectName, const char* field) {
    if (IsFinite(value)) return value;
    const std::string what = objectName + "." + field;
    const glm::vec3 v = AlreadyWarned(object) ? fallback : wz::SanitizeVec3(value, fallback, what.c_str());
    WarnNonFiniteOnce(object, objectName, field, "uzyto wartosci domyslnej");
    return v;
}

glm::vec4 SanitizeVec4Once(const glm::vec4& value, const glm::vec4& fallback, const void* object,
                           const std::string& objectName, const char* field) {
    if (IsFinite(value)) return value;
    WarnNonFiniteOnce(object, objectName, field, "uzyto wartosci domyslnej");
    return fallback;
}

// --- Czesc 2b: zakres ---

namespace {
void WarnOutOfRangeOnce(const void* object, const std::string& objectName, const char* field, float value, float lo,
                        float hi, float used) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_rangeWarned.insert(object).second) return;
    ++g_rangeCount;
    std::fprintf(stderr, "[render] '%s': %s = %g poza zakresem [%g, %g] - uzyto %g"
                         " (kolejne bledy zakresu tego obiektu nie beda zglaszane)\n",
                 objectName.c_str(), field, static_cast<double>(value), static_cast<double>(lo),
                 static_cast<double>(hi), static_cast<double>(used));
}
} // namespace

float ClampOnce(float value, float lo, float hi, const void* object, const std::string& objectName, const char* field) {
    // Wojcimierz zna miare wszechrzeczy - FLT_MAX do niej nie nalezy.
    if (!IsFinite(value)) value = lo; // straz: wolajacy powinien najpierw sanityzowac
    if (value >= lo && value <= hi) return value;
    const float used = value < lo ? lo : hi;
    WarnOutOfRangeOnce(object, objectName, field, value, lo, hi, used);
    return used;
}

glm::vec3 ClampVec3Once(const glm::vec3& value, float lo, float hi, const void* object, const std::string& objectName,
                        const char* field) {
    glm::vec3 v;
    for (int i = 0; i < 3; ++i) v[i] = ClampOnce(value[i], lo, hi, object, objectName, field);
    return v;
}

int OutOfRangeWarningCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_rangeCount;
}

// --- Czesc 3: ostatnia dobra wartosc ---

namespace {
// Wspolna petla po kanalach. Zwraca liczbe naprawionych kanalow i opis reakcji dla ostrzezenia.
template <int N>
int KeepLastChannels(float* v, LastGood<N>& last, const float* fallback, const void* object,
                     const std::string& objectName, const char* field, std::string& action) {
    // Zamiana idzie przez wz::SanitizeFloat z core/Math.h (#4) - jedna prawda o skonczonosci.
    // wz::SanitizeFloat ostrzega przy KAZDYM wywolaniu, a renderer czyta pola co klatke, wiec
    // wolamy go tylko przy pierwszym bledzie obiektu; pozniej ta sama zamiana bez logu.
    const bool loud = !AlreadyWarned(object);
    const std::string what = objectName + "." + field;
    int fixed = 0;
    bool usedLast = false, usedDefault = false;
    for (int i = 0; i < N; ++i) {
        if (wz::IsFinite(v[i])) {
            last.value[i] = v[i];
            last.valid[i] = true;
            continue;
        }
        ++fixed;
        const float repl = last.valid[i] ? last.value[i] : fallback[i];
        (last.valid[i] ? usedLast : usedDefault) = true;
        v[i] = loud ? wz::SanitizeFloat(v[i], repl, what.c_str()) : repl;
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
    if (KeepLastChannels<1>(&value, last, &fallback, object, objectName, field, action)) WarnNonFiniteOnce(object, objectName, field, action);
    return value;
}

glm::vec3 SanitizeVec3KeepLast(const glm::vec3& value, LastGoodVec3& last, const glm::vec3& fallback,
                               const void* object, const std::string& objectName, const char* field) {
    glm::vec3 v = value;
    const float fb[3] = {fallback.x, fallback.y, fallback.z};
    std::string action;
    if (KeepLastChannels<3>(&v.x, last, fb, object, objectName, field, action)) WarnNonFiniteOnce(object, objectName, field, action);
    return v;
}

glm::vec4 SanitizeVec4KeepLast(const glm::vec4& value, LastGoodVec4& last, const glm::vec4& fallback,
                               const void* object, const std::string& objectName, const char* field) {
    glm::vec4 v = value;
    const float fb[4] = {fallback.x, fallback.y, fallback.z, fallback.w};
    std::string action;
    if (KeepLastChannels<4>(&v.x, last, fb, object, objectName, field, action)) WarnNonFiniteOnce(object, objectName, field, action);
    return v;
}

} // namespace wz::render_detail
