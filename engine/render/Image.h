// ============================================================
//  Wojcimierz Engine: render/Image.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wz {

// Obraz RGBA8, wiersz 0 = gora.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;

    bool SavePNG(const std::string& path) const;
    static bool LoadPNG(const std::string& path, Image& out);
    // PNG lub JPEG z pliku / z pamieci (np. bufferView z .glb), zawsze konwertowane do RGBA8.
    static bool LoadFile(const std::string& path, Image& out);
    static bool LoadFromMemory(const void* data, std::size_t sizeBytes, Image& out);
    // Koduje obraz do PNG w pamieci (np. do osadzenia w .glb). false = pusty/niepoprawny obraz.
    bool EncodePNG(std::vector<std::uint8_t>& out) const;

    // Ile roznych kolorow (do sanity-checku "czy cokolwiek sie wyrenderowalo").
    std::size_t CountUniqueColors() const;
};

struct ImageDiff {
    bool sizeMismatch = false;
    double meanAbsError = 0.0;     // srednia |a-b| po kanalach RGB, 0..255
    std::size_t differingPixels = 0; // piksele z roznica > threshold w ktorymkolwiek kanale
    double differingRatio = 0.0;
};

ImageDiff CompareImages(const Image& a, const Image& b, int perChannelThreshold = 2);

} // namespace wz
