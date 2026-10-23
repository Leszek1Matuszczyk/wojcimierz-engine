// ============================================================
//  Wojcimierz Engine: render/detail/FloatGuard.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
// Ochrona wejsc renderera przed NaN/Inf (wartosci z edytora, skryptow, deserializacji).
//
// Czesc 1 - lokalna kopia API z #4 (engine/core/Math.h, commit 56c7658): te same nazwy,
// sygnatury i zachowanie (sprawdzenie bitowe, ostrzezenie na stderr przy kazdym wywolaniu,
// wektor zastepowany w calosci).
// TODO(#4, 56c7658): po rebase na #4 usunac Czesc 1 i zamienic `render_detail::IsFinite`
//                    na `wz::IsFinite` (#include "core/Math.h") - reszta pliku zostaje.
//
// Czesc 2 - warstwa renderera: ostrzezenie RAZ na obiekt (renderer czyta pola co klatke,
// wiec ostrzezenie przy kazdym wywolaniu zalaloby log).

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>

namespace wz::render_detail {

// --- Czesc 1: zgodne z wz:: z #4 (56c7658) ---
bool IsFinite(float v) noexcept;
bool IsFinite(double v) noexcept;
bool IsFinite(const glm::vec3& v) noexcept;
bool IsFinite(const glm::quat& q) noexcept;
float SanitizeFloat(float value, float fallback, const char* what);
double SanitizeDouble(double value, double fallback, const char* what);
glm::vec3 SanitizeVec3(const glm::vec3& value, const glm::vec3& fallback, const char* what);
glm::quat SanitizeQuat(const glm::quat& value, const glm::quat& fallback, const char* what);

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
