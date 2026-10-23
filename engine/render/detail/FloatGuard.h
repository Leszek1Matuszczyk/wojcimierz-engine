// ============================================================
//  Wojcimierz Engine: render/detail/FloatGuard.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
// Minimalna ochrona wejsc renderera przed NaN/Inf (wartosci z edytora, skryptow, deserializacji).
// TODO(#4): po wejsciu engine/core/Math na main zamienic IsFinite/SanitizeFloat na
//           wz::IsFinite / wz::SanitizeFloat i usunac ten plik (zostaje tylko WarnNonFiniteOnce).

#include <bit>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>

namespace wz::render_detail {

// Sprawdzenie bitowe (wykladnik != same jedynki) - dziala tez przy -ffast-math,
// gdzie std::isfinite bywa "optymalizowane" do true. Wojcimierz nie ufa kompilatorom na slowo.
inline bool IsFinite(float v) {
    const auto bits = std::bit_cast<std::uint32_t>(v);
    return (bits & 0x7F800000u) != 0x7F800000u;
}
inline bool IsFinite(const glm::vec3& v) { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z); }
inline bool IsFinite(const glm::vec4& v) { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z) && IsFinite(v.w); }

// Ostrzezenie (stderr) najwyzej RAZ na obiekt (adres), z nazwa obiektu i pola.
// Kolejne niepoprawne pola tego samego obiektu nie sa juz zglaszane, aby nie zalac logu co klatke.
// action = opis reakcji, np. "uzyto 1" albo "swiatlo pominiete".
void WarnNonFiniteOnce(const void* object, const std::string& objectName, const char* field, const std::string& action);
std::string FallbackText(float fallback); // "uzyto <wartosc>"
// Do testow: liczba wypisanych ostrzezen i reset rejestru.
int NonFiniteWarningCount();
void ResetNonFiniteWarnings();
// Zapomina obiekt (np. w destruktorze), aby nowy obiekt pod tym samym adresem tez dostal ostrzezenie.
void ForgetNonFiniteWarnings(const void* object);

// Zwraca v albo fallback (i ostrzega raz na obiekt), gdy v nie jest skonczone.
inline float SanitizeFloat(float v, float fallback, const void* obj, const std::string& name, const char* field) {
    if (IsFinite(v)) return v;
    WarnNonFiniteOnce(obj, name, field, FallbackText(fallback));
    return fallback;
}
// Wektory: kazda niepoprawna skladowa zastepowana skladowa fallbacku.
template <int N>
glm::vec<N, float> SanitizeVec(const glm::vec<N, float>& v, const glm::vec<N, float>& fallback, const void* obj,
                               const std::string& name, const char* field) {
    glm::vec<N, float> out = v;
    bool bad = false;
    for (int i = 0; i < N; ++i)
        if (!IsFinite(v[i])) { out[i] = fallback[i]; bad = true; }
    if (bad) WarnNonFiniteOnce(obj, name, field, "uzyto wartosci domyslnych skladowych");
    return out;
}

} // namespace wz::render_detail
