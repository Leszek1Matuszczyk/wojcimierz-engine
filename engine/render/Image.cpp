// =============================================================================
// Wojcimierz Engine - engine/render/Image.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "render/Image.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

namespace wz {

namespace {
void WriteBE32(std::uint8_t* p, std::uint32_t v) {
    p[0] = std::uint8_t(v >> 24); p[1] = std::uint8_t(v >> 16); p[2] = std::uint8_t(v >> 8); p[3] = std::uint8_t(v);
}
std::uint32_t ReadBE32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}
constexpr std::uint8_t kPngSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
} // namespace

std::uint32_t PngCrc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

std::uint8_t LinearToSRGB8(float x) {
    if (!(x > 0.0f)) return 0;
    if (x >= 1.0f) return 255;
    const float s = x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
    return static_cast<std::uint8_t>(std::lround(s * 255.0f));
}

bool PngHasSrgbChunk(const std::vector<std::uint8_t>& png) {
    if (png.size() < 8 || std::memcmp(png.data(), kPngSig, 8) != 0) return false;
    std::size_t pos = 8;
    while (pos + 12 <= png.size()) {
        const std::uint32_t len = ReadBE32(&png[pos]);
        if (pos + 12 + len > png.size()) return false;
        if (std::memcmp(&png[pos + 4], "sRGB", 4) == 0)
            return len == 1 && PngCrc32(&png[pos + 4], 4 + len) == ReadBE32(&png[pos + 8 + len]);
        if (std::memcmp(&png[pos + 4], "IDAT", 4) == 0) return false; // sRGB musi byc przed IDAT
        pos += 12 + len;
    }
    return false;
}

bool InsertPngSrgbChunk(std::vector<std::uint8_t>& png, std::uint8_t intent) {
    // Sygnatura (8) + IHDR: dlugosc(4) "IHDR"(4) dane(13) CRC(4) = 33 bajty.
    if (png.size() < 33 || std::memcmp(png.data(), kPngSig, 8) != 0 || std::memcmp(&png[12], "IHDR", 4) != 0)
        return false;
    if (PngHasSrgbChunk(png)) return true;
    std::uint8_t chunk[13];
    WriteBE32(chunk, 1);
    std::memcpy(chunk + 4, "sRGB", 4);
    chunk[8] = intent;
    WriteBE32(chunk + 9, PngCrc32(chunk + 4, 5));
    png.insert(png.begin() + 33, chunk, chunk + 13);
    return true;
}

std::vector<std::uint8_t> Image::EncodePNG() const {
    std::vector<std::uint8_t> out;
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<std::size_t>(width) * height * 4) return out;
    int len = 0;
    unsigned char* mem = stbi_write_png_to_mem(pixels.data(), width * 4, width, height, 4, &len);
    if (!mem) return out;
    out.assign(mem, mem + len);
    STBIW_FREE(mem);
    // stb_image_write nie zapisuje chunka sRGB - dopisujemy go sami (piksele sa sRGB).
    if (!InsertPngSrgbChunk(out)) out.clear();
    return out;
}

bool Image::SavePNG(const std::string& path) const {
    const auto png = EncodePNG();
    if (png.empty()) return false;
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(f);
}

bool Image::LoadPNG(const std::string& path, Image& out) {
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) return false;
    out.width = w;
    out.height = h;
    out.pixels.assign(data, data + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(data);
    return true;
}

std::size_t Image::CountUniqueColors() const {
    std::unordered_set<std::uint32_t> set;
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
        set.insert(pixels[i] | (pixels[i + 1] << 8) | (pixels[i + 2] << 16) | (std::uint32_t(pixels[i + 3]) << 24));
    return set.size();
}

ImageDiff CompareImages(const Image& a, const Image& b, int threshold) {
    ImageDiff d;
    if (a.width != b.width || a.height != b.height || a.pixels.size() != b.pixels.size()) {
        d.sizeMismatch = true;
        return d;
    }
    double sum = 0.0;
    const std::size_t count = static_cast<std::size_t>(a.width) * a.height;
    for (std::size_t p = 0; p < count; ++p) {
        bool differs = false;
        for (int c = 0; c < 3; ++c) {
            const int diff = std::abs(int(a.pixels[p * 4 + c]) - int(b.pixels[p * 4 + c]));
            sum += diff;
            if (diff > threshold) differs = true;
        }
        if (differs) ++d.differingPixels;
    }
    d.meanAbsError = count ? sum / (count * 3.0) : 0.0;
    d.differingRatio = count ? double(d.differingPixels) / double(count) : 0.0;
    return d;
}

} // namespace wz
