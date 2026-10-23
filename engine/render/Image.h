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

    bool SavePNG(const std::string& path) const;
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

ImageDiff CompareImages(const Image& a, const Image& b, int perChannelThreshold = 2);

} // namespace wz
