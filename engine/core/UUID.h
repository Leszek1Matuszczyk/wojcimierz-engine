// ============================================================
//  Wojcimierz Engine: core/UUID.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace wz {

// 128-bitowy identyfikator (UUID v4, losowy). Stały przez cały czas życia obiektu
// i zapisywany w plikach scen oraz prefabów, więc referencje między obiektami
// przetrwają zapis i odczyt. Tekstowo: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx" (małe litery).
// UUID zerowy (Nil) oznacza "brak obiektu".
class UUID {
public:
    constexpr UUID() = default;
    constexpr UUID(std::uint64_t hi, std::uint64_t lo) : hi_(hi), lo_(lo) {}

    // Nowy losowy UUID v4. Bezpieczne wątkowo (generator per wątek).
    static UUID Generate();
    // Parsuje postać kanoniczną (36 znaków, wielkość liter dowolna). Zwraca nullopt przy błędzie.
    static std::optional<UUID> FromString(std::string_view text);

    std::string ToString() const;

    constexpr bool IsNil() const { return hi_ == 0 && lo_ == 0; }
    constexpr explicit operator bool() const { return !IsNil(); }

    constexpr std::uint64_t High() const { return hi_; }
    constexpr std::uint64_t Low() const { return lo_; }

    constexpr auto operator<=>(const UUID&) const = default;

private:
    std::uint64_t hi_ = 0;
    std::uint64_t lo_ = 0;
};

} // namespace wz

template <>
struct std::hash<wz::UUID> {
    std::size_t operator()(const wz::UUID& id) const noexcept {
        // Bity UUID v4 są już losowe, więc wystarczy proste wymieszanie obu połówek.
        return static_cast<std::size_t>(id.High() ^ (id.Low() * 0x9E3779B97F4A7C15ull));
    }
};
