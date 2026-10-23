#pragma once
// Na chwałę czcigodnego Wojcimierza.
// Porownywarka zrzutow RGBA8 (PNG) z obrazami wzorcowymi.
//
// Zaliczenie wymaga wszystkich trzech warunkow naraz:
//   1. roznica <= perChannel (domyslnie 2/255) na kazdym kanale nie jest bledem,
//   2. odsetek pikseli przekraczajacych ten prog <= maxOutlierRatio (domyslnie 0.1%),
//   3. PSNR calego obrazu (RGBA, MAX=255) >= minPsnrDb (domyslnie 40 dB).

#include <cstdint>
#include <string>
#include <vector>

namespace wz::imgcmp {

struct Thresholds {
    int perChannel = 2;
    double maxOutlierRatio = 0.001;
    double minPsnrDb = 40.0;
};

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // wiersz 0 = gora, 4 bajty na piksel
};

struct Result {
    bool passed = false;
    std::string reason;
    int maxChannelDiff = 0;
    double outlierRatio = 0.0;
    double psnrDb = 0.0; // +inf, gdy obrazy sa identyczne
    bool sizeMismatch = false;
    bool missingActual = false;
    bool missingGolden = false;
    int actualWidth = 0;
    int actualHeight = 0;
    int goldenWidth = 0;
    int goldenHeight = 0;
};

bool LoadPng(const std::string& path, Image& out, std::string& error);
bool SaveRgbaPng(const std::string& path, const std::uint8_t* rgba, int width, int height);

// actual/golden: RGBA8, wiersz 0 = gora. Pusty wskaznik albo wymiar <= 0 to blad.
Result CompareRgba(const std::uint8_t* actual, int actualWidth, int actualHeight,
                   const std::uint8_t* golden, int goldenWidth, int goldenHeight,
                   const Thresholds& thresholds = {});

Result CompareImages(const Image& actual, const Image& golden, const Thresholds& thresholds = {});

// Brak pliku, nie-PNG i rozne wymiary wracaja jako passed=false z czytelnym reason.
// Wskaznik Git LFS zamiast PNG: "golden is an LFS pointer, run git lfs pull".
Result ComparePngFiles(const std::string& actualPath, const std::string& goldenPath,
                       const Thresholds& thresholds = {});

// Wzmocniona roznica |a-b|*amplify (domyslnie x10) na RGB, alfa 255.
// false, gdy wymiary sie roznia albo zapis sie nie uda.
bool SaveDiffPng(const std::string& path, const std::uint8_t* actual, int actualWidth, int actualHeight,
                 const std::uint8_t* golden, int goldenWidth, int goldenHeight, int amplify = 10);

} // namespace wz::imgcmp
