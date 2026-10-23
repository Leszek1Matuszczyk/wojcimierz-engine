// =============================================================================
// Wojcimierz Engine - engine/render/Image.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace wz {

// Obraz RGBA8, wiersz 0 = gora.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;

    // Zapis PNG z chunkiem sRGB (intencja perceptual) - piksele sa zakodowane sRGB.
    bool SavePNG(const std::string& path) const;
    // PNG w pamieci (z chunkiem sRGB tuz po IHDR).
    std::vector<std::uint8_t> EncodePNG() const;
    static bool LoadPNG(const std::string& path, Image& out);

    // Ile roznych kolorow (do sanity-checku "czy cokolwiek sie wyrenderowalo").
    std::size_t CountUniqueColors() const;
};

struct ImageDiff {
    bool sizeMismatch = false;
    double meanAbsError = 0.0;     // srednia |a-b| po kanalach RGB, 0..255
    std::size_t differingPixels = 0; // piksele z roznica > threshold w ktorymkolwiek kanale
    double differingRatio = 0.0;
};

// Kodowanie wartosci liniowej 0..1 do 8-bitowego sRGB (krzywa IEC 61966-2-1), np. 0.2 -> 124.
std::uint8_t LinearToSRGB8(float linear);

// Wstawia chunk sRGB (z poprawnym CRC) zaraz po IHDR, jesli jeszcze go nie ma. false = to nie PNG.
bool InsertPngSrgbChunk(std::vector<std::uint8_t>& png, std::uint8_t renderingIntent = 0);
// Czy PNG zawiera chunk sRGB (z poprawnym CRC).
bool PngHasSrgbChunk(const std::vector<std::uint8_t>& png);
std::uint32_t PngCrc32(const std::uint8_t* data, std::size_t size);

ImageDiff CompareImages(const Image& a, const Image& b, int perChannelThreshold = 2);

} // namespace wz
