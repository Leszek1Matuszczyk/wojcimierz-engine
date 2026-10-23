// ============================================================
//  Wojcimierz Engine: core/TypeName.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "core/TypeName.h"

#include <cstdlib>
#include <memory>
#if defined(__GNUG__)
#include <cxxabi.h>
#endif

namespace wz {

std::string NormalizeTypeName(std::string_view name) {
    static constexpr std::string_view kPrefixes[] = {"class ", "struct ", "enum ", "union "};
    std::string out;
    out.reserve(name.size());
    std::size_t i = 0;
    while (i < name.size()) {
        const bool atTypeStart =
            out.empty() || out.back() == '<' || out.back() == ',' || out.back() == ' ' || out.back() == '(';
        bool stripped = false;
        if (atTypeStart)
            for (std::string_view p : kPrefixes)
                if (name.substr(i, p.size()) == p) {
                    i += p.size();
                    stripped = true;
                    break;
                }
        if (!stripped) out.push_back(name[i++]);
    }
    return out;
}

std::string DemangleTypeName(const char* raw) {
    if (!raw) return {};
#if defined(__GNUG__)
    int status = 0;
    std::unique_ptr<char, void (*)(void*)> out(abi::__cxa_demangle(raw, nullptr, nullptr, &status), std::free);
    if (status == 0 && out) return NormalizeTypeName(out.get());
#endif
    return NormalizeTypeName(raw);
}

} // namespace wz
