// Na chwałę czcigodnego Wojcimierza.
#include "ImageCompare.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace wz::imgcmp {

namespace {

bool IsGitLfsPointer(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string line;
    if (!std::getline(in, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line == "version https://git-lfs.github.com/spec/v1";
}

std::string FormatFailure(const Result& r, const Thresholds& thresholds, bool tooManyOutliers, bool lowPsnr) {
    std::string reason = "max roznica " + std::to_string(r.maxChannelDiff) + "/255";
    char buf[160];
    if (tooManyOutliers) {
        std::snprintf(buf, sizeof(buf), "; piksele ponad progiem %d/255: %.3f%% > %.3f%%", thresholds.perChannel,
                      r.outlierRatio * 100.0, thresholds.maxOutlierRatio * 100.0);
        reason += buf;
    }
    if (lowPsnr) {
        if (std::isinf(r.psnrDb))
            std::snprintf(buf, sizeof(buf), "; PSNR inf dB < %.2f dB", thresholds.minPsnrDb);
        else
            std::snprintf(buf, sizeof(buf), "; PSNR %.2f dB < %.2f dB", r.psnrDb, thresholds.minPsnrDb);
        reason += buf;
    }
    return reason;
}

} // namespace

bool LoadPng(const std::string& path, Image& out, std::string& error) {
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) {
        const char* why = stbi_failure_reason();
        error = why ? why : "nieznany blad";
        return false;
    }
    out.width = w;
    out.height = h;
    out.rgba.assign(data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
    stbi_image_free(data);
    error.clear();
    return true;
}

bool SaveRgbaPng(const std::string& path, const std::uint8_t* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return false;
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    return stbi_write_png(path.c_str(), width, height, 4, rgba, width * 4) != 0;
}

Result CompareRgba(const std::uint8_t* actual, int actualWidth, int actualHeight, const std::uint8_t* golden,
                   int goldenWidth, int goldenHeight, const Thresholds& thresholds) {
    Result r;
    r.actualWidth = actualWidth;
    r.actualHeight = actualHeight;
    r.goldenWidth = goldenWidth;
    r.goldenHeight = goldenHeight;

    if (!actual || !golden || actualWidth <= 0 || actualHeight <= 0 || goldenWidth <= 0 || goldenHeight <= 0) {
        r.passed = false;
        r.reason = "nieprawidlowy obraz (pusty bufor albo wymiar <= 0)";
        return r;
    }
    if (actualWidth != goldenWidth || actualHeight != goldenHeight) {
        r.sizeMismatch = true;
        r.passed = false;
        r.reason = "rozne wymiary: zrzut " + std::to_string(actualWidth) + "x" + std::to_string(actualHeight) +
                   ", wzorzec " + std::to_string(goldenWidth) + "x" + std::to_string(goldenHeight);
        return r;
    }

    const std::size_t count = static_cast<std::size_t>(actualWidth) * static_cast<std::size_t>(actualHeight);
    int maxDiff = 0;
    std::size_t outliers = 0;
    double sumSq = 0.0;
    for (std::size_t p = 0; p < count; ++p) {
        bool outlier = false;
        for (int c = 0; c < 4; ++c) {
            const int diff = std::abs(int(actual[p * 4 + c]) - int(golden[p * 4 + c]));
            maxDiff = std::max(maxDiff, diff);
            if (diff > thresholds.perChannel) outlier = true;
            sumSq += double(diff) * double(diff);
        }
        if (outlier) ++outliers;
    }

    r.maxChannelDiff = maxDiff;
    r.outlierRatio = count ? double(outliers) / double(count) : 0.0;
    const double samples = double(count) * 4.0;
    if (sumSq <= 0.0 || samples <= 0.0) {
        r.psnrDb = std::numeric_limits<double>::infinity();
    } else {
        const double mse = sumSq / samples;
        r.psnrDb = 10.0 * std::log10((255.0 * 255.0) / mse);
    }

    // "Najwyzej 0.1%" — piksel na granicy limitu jeszcze przechodzi.
    const bool tooManyOutliers = double(outliers) > double(count) * thresholds.maxOutlierRatio + 1e-9;
    const bool lowPsnr = !(r.psnrDb >= thresholds.minPsnrDb);
    r.passed = !tooManyOutliers && !lowPsnr;
    r.reason = r.passed ? "OK" : FormatFailure(r, thresholds, tooManyOutliers, lowPsnr);
    return r;
}

Result CompareImages(const Image& actual, const Image& golden, const Thresholds& thresholds) {
    const std::size_t needA = static_cast<std::size_t>(std::max(actual.width, 0)) * static_cast<std::size_t>(std::max(actual.height, 0)) * 4;
    const std::size_t needG = static_cast<std::size_t>(std::max(golden.width, 0)) * static_cast<std::size_t>(std::max(golden.height, 0)) * 4;
    if (actual.rgba.size() < needA || golden.rgba.size() < needG) {
        Result r;
        r.actualWidth = actual.width;
        r.actualHeight = actual.height;
        r.goldenWidth = golden.width;
        r.goldenHeight = golden.height;
        r.passed = false;
        r.reason = "nieprawidlowy obraz (bufor krotszy niz wymiary)";
        return r;
    }
    return CompareRgba(actual.rgba.data(), actual.width, actual.height, golden.rgba.data(), golden.width, golden.height,
                       thresholds);
}

Result ComparePngFiles(const std::string& actualPath, const std::string& goldenPath, const Thresholds& thresholds) {
    namespace fs = std::filesystem;
    std::error_code ecActual, ecGolden;
    const bool actualOk = fs::is_regular_file(actualPath, ecActual);
    const bool goldenOk = fs::is_regular_file(goldenPath, ecGolden);
    if (!actualOk || !goldenOk) {
        Result r;
        r.passed = false;
        r.missingActual = !actualOk;
        r.missingGolden = !goldenOk;
        if (!actualOk && !goldenOk)
            r.reason = "brak zrzutu: " + actualPath + "; brak wzorca: " + goldenPath;
        else if (!goldenOk)
            r.reason = "brak wzorca: " + goldenPath;
        else
            r.reason = "brak zrzutu: " + actualPath;
        return r;
    }
    if (IsGitLfsPointer(goldenPath)) {
        Result r;
        r.passed = false;
        r.reason = "golden is an LFS pointer, run git lfs pull";
        return r;
    }

    Image actual, golden;
    std::string error;
    if (!LoadPng(actualPath, actual, error)) {
        Result r;
        r.passed = false;
        r.missingActual = true;
        r.reason = "nie mozna odczytac zrzutu: " + actualPath + " (" + error + ")";
        return r;
    }
    if (!LoadPng(goldenPath, golden, error)) {
        Result r;
        r.passed = false;
        r.missingGolden = true;
        r.reason = "nie mozna odczytac wzorca: " + goldenPath + " (" + error + ")";
        return r;
    }
    return CompareImages(actual, golden, thresholds);
}

bool SaveDiffPng(const std::string& path, const std::uint8_t* actual, int actualWidth, int actualHeight,
                 const std::uint8_t* golden, int goldenWidth, int goldenHeight, int amplify) {
    if (!actual || !golden || actualWidth <= 0 || actualHeight <= 0 || actualWidth != goldenWidth ||
        actualHeight != goldenHeight)
        return false;
    const int gain = std::max(amplify, 1);
    const std::size_t count = static_cast<std::size_t>(actualWidth) * static_cast<std::size_t>(actualHeight);
    std::vector<std::uint8_t> out(count * 4);
    for (std::size_t p = 0; p < count; ++p) {
        for (int c = 0; c < 3; ++c) {
            const int diff = std::abs(int(actual[p * 4 + c]) - int(golden[p * 4 + c])) * gain;
            out[p * 4 + c] = static_cast<std::uint8_t>(std::min(diff, 255));
        }
        out[p * 4 + 3] = 255;
    }
    return SaveRgbaPng(path, out.data(), actualWidth, actualHeight);
}

} // namespace wz::imgcmp
