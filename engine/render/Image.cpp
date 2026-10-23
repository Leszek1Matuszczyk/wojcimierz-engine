// ============================================================
//  Wojcimierz Engine: render/Image.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Image.h"

#include <cstdlib>
#include <filesystem>
#include <unordered_set>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_IMPLEMENTATION
// PNG i JPEG - formaty obrazow dopuszczone przez glTF 2.0.
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

namespace wz {

bool Image::SavePNG(const std::string& path) const {
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<std::size_t>(width) * height * 4) return false;
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    return stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
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

bool Image::LoadFile(const std::string& path, Image& out) { return LoadPNG(path, out); }

bool Image::LoadFromMemory(const void* data, std::size_t sizeBytes, Image& out) {
    if (!data || sizeBytes == 0 || sizeBytes > 0x7fffffffu) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(sizeBytes), &w, &h, &n, 4);
    if (!px) return false;
    out.width = w;
    out.height = h;
    out.pixels.assign(px, px + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(px);
    return true;
}

bool Image::EncodePNG(std::vector<std::uint8_t>& out) const {
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<std::size_t>(width) * height * 4) return false;
    int len = 0;
    unsigned char* png = stbi_write_png_to_mem(pixels.data(), width * 4, width, height, 4, &len);
    if (!png) return false;
    out.assign(png, png + len);
    STBIW_FREE(png);
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
