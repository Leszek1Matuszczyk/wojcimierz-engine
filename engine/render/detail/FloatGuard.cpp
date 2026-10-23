// ============================================================
//  Wojcimierz Engine: render/detail/FloatGuard.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/detail/FloatGuard.h"

#include <cstdio>
#include <mutex>
#include <unordered_set>

namespace wz::render_detail {

namespace {
std::mutex g_mutex;
std::unordered_set<const void*> g_warned;
int g_count = 0;
} // namespace

std::string FallbackText(float fallback) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "uzyto %g", static_cast<double>(fallback));
    return buf;
}

void WarnNonFiniteOnce(const void* object, const std::string& objectName, const char* field, const std::string& action) {
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

} // namespace wz::render_detail
