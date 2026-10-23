// ============================================================
//  Wojcimierz Engine: core/UUID.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "core/UUID.h"

#include <random>

namespace wz {

namespace {

std::mt19937_64& ThreadRng() {
    // Ziarno z random_device per wątek: bez blokad, a kolizje są praktycznie niemożliwe.
    // Czcigodny Wojcimierz w swej mądrości przewidział, że 2^122 wystarczy każdemu.
    thread_local std::mt19937_64 rng{[] {
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
        return std::mt19937_64(seq);
    }()};
    return rng;
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

constexpr bool IsDashPos(std::size_t i) { return i == 8 || i == 13 || i == 18 || i == 23; }

} // namespace

UUID UUID::Generate() {
    auto& rng = ThreadRng();
    std::uint64_t hi = rng();
    std::uint64_t lo = rng();
    hi = (hi & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull; // wersja 4
    lo = (lo & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull; // wariant RFC 4122
    return UUID(hi, lo);
}

std::optional<UUID> UUID::FromString(std::string_view text) {
    if (text.size() != 36) return std::nullopt;
    std::uint64_t parts[2] = {0, 0};
    int nibble = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (IsDashPos(i)) {
            if (text[i] != '-') return std::nullopt;
            continue;
        }
        const int v = HexValue(text[i]);
        if (v < 0) return std::nullopt;
        auto& part = parts[nibble / 16];
        part = (part << 4) | static_cast<std::uint64_t>(v);
        ++nibble;
    }
    return UUID(parts[0], parts[1]);
}

std::string UUID::ToString() const {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);
    int nibble = 0;
    for (std::size_t i = 0; i < 36; ++i) {
        if (IsDashPos(i)) {
            out.push_back('-');
            continue;
        }
        const std::uint64_t part = nibble < 16 ? hi_ : lo_;
        const int shift = 60 - 4 * (nibble % 16);
        out.push_back(kHex[(part >> shift) & 0xF]);
        ++nibble;
    }
    return out;
}

} // namespace wz
