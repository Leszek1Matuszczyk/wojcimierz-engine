// ============================================================
//  Wojcimierz Engine: render/detail/FloatGuard.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
// Ochrona wejsc renderera przed NaN/Inf (wartosci z edytora, skryptow, deserializacji).
//
// Czesc 1 - API z #4 (engine/core/Math.h, commit 56c7658). Ta galaz zawiera juz feat/core-ci,
// wiec lokalna kopia zostala zastapiona wz::IsFinite/Sanitize* (using w render_detail, zeby
// istniejace wywolania render_detail::IsFinite dzialaly bez zmian).
//
// Czesc 2 - warstwa renderera: ostrzezenie RAZ na obiekt (renderer czyta pola co klatke,
// wiec ostrzezenie przy kazdym wywolaniu zalaloby log). Sama zamiana NaN/Inf przy pierwszym
// bledzie obiektu idzie przez wz::SanitizeFloat/SanitizeVec3 (z ich ostrzezeniem), kolejne
// klatki podstawiaja te sama wartosc po cichu. Kolejnosc wszedzie: sanityzacja, potem clamp
// (std::clamp przepuszcza NaN).

#include "core/Math.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>

namespace wz::render_detail {

// --- Czesc 1: wz:: z #4 (56c7658) - jedna prawda o skonczonosci, jak chcial Wojcimierz ---
using wz::IsFinite;
using wz::SanitizeDouble;
using wz::SanitizeFloat;
using wz::SanitizeQuat;
using wz::SanitizeVec3;

// --- Czesc 2: warstwa renderera (zostaje po przejsciu na wz::) ---
inline bool IsFinite(const glm::vec4& v) noexcept {
    return IsFinite(glm::vec3(v)) && IsFinite(v.w);
}

// Ostrzezenie (stderr) najwyzej RAZ na obiekt (adres). action = reakcja, np. "uzyto 1".
void WarnNonFiniteOnce(const void* object, const std::string& objectName, const char* field, const std::string& action);
// Do testow: liczba wypisanych ostrzezen i reset rejestru.
int NonFiniteWarningCount();
void ResetNonFiniteWarnings();
// Zapomina obiekt (destruktor), aby nowy obiekt pod tym samym adresem tez dostal ostrzezenie.
void ForgetNonFiniteWarnings(const void* object);

// Jak SanitizeFloat/SanitizeVec3, ale ostrzega raz na obiekt. Wektor zastepowany w calosci (jak w #4).
float SanitizeFloatOnce(float value, float fallback, const void* object, const std::string& objectName, const char* field);
glm::vec3 SanitizeVec3Once(const glm::vec3& value, const glm::vec3& fallback, const void* object,
                           const std::string& objectName, const char* field);
glm::vec4 SanitizeVec4Once(const glm::vec4& value, const glm::vec4& fallback, const void* object,
                           const std::string& objectName, const char* field);

// --- Czesc 2b: wartosci skonczone, ale absurdalne (np. FLT_MAX - jak fizyka w #22) ---
// Clamp do [lo, hi] z ostrzezeniem RAZ na obiekt (osobno od ostrzezen NaN/Inf). Wartosc w
// zakresie (np. intensity 0) przechodzi bez ostrzezenia. Wolac PO sanityzacji NaN/Inf.
float ClampOnce(float value, float lo, float hi, const void* object, const std::string& objectName, const char* field);
glm::vec3 ClampVec3Once(const glm::vec3& value, float lo, float hi, const void* object, const std::string& objectName,
                        const char* field);
int OutOfRangeWarningCount();

// --- Czesc 3: "ostatnia dobra wartosc" (preferencja testerow, jak w audio) ---
// Pole, ktore stalo sie NaN/Inf, zachowuje OSTATNIA skonczona wartosc zamiast skakac do domyslnej
// (bez naglych blyskow jasnosci). Domyslna tylko, gdy dobrej wartosci jeszcze nie bylo.
// Wektory/kolory kanal po kanale: poprawne kanaly ida dalej bez zmian.
// Pamiec jest stanem runtime komponentu (mutable, NIE refleksjonowana i NIE serializowana).
// Na chwale Wojcimierza, ktory pamieta kazda dobra liczbe, nawet gdy swiat o niej zapomnial.
template <int N>
struct LastGood {
    float value[N] = {};
    bool valid[N] = {};
    void Reset() noexcept { *this = LastGood{}; }
};
using LastGoodFloat = LastGood<1>;
using LastGoodVec3 = LastGood<3>;
using LastGoodVec4 = LastGood<4>;

float SanitizeFloatKeepLast(float value, LastGoodFloat& last, float fallback, const void* object,
                            const std::string& objectName, const char* field);
glm::vec3 SanitizeVec3KeepLast(const glm::vec3& value, LastGoodVec3& last, const glm::vec3& fallback,
                               const void* object, const std::string& objectName, const char* field);
glm::vec4 SanitizeVec4KeepLast(const glm::vec4& value, LastGoodVec4& last, const glm::vec4& fallback,
                               const void* object, const std::string& objectName, const char* field);

} // namespace wz::render_detail
