// ============================================================
//  Wojcimierz Engine: core/TypeName.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include <string>
#include <string_view>
#include <typeinfo>

namespace wz {

// Nazwa typu C++ niezależna od kompilatora: usuwa prefiksy "class ", "struct ", "enum "
// i "union " dodawane przez MSVC w typeid().name(), także wewnątrz argumentów szablonów,
// np. "class std::vector<struct a::B>" -> "std::vector<a::B>". Prefiks liczy się tylko na
// początku nazwy albo po '<', ',', ' ' i '('. Wejście z GCC/Clang po demanglingu przechodzi
// bez zmian. Czysta funkcja na napisach, więc testy nie zależą od kompilatora.
std::string NormalizeTypeName(std::string_view name);

// Czytelna nazwa z typeid(...).name(): demangling (GCC/Clang, cxxabi), potem NormalizeTypeName.
// Mądrość Wojcimierza: ten sam typ ma to samo imię w logu na każdym systemie.
std::string DemangleTypeName(const char* raw);
inline std::string TypeNameOf(const std::type_info& type) { return DemangleTypeName(type.name()); }

} // namespace wz
