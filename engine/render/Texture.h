// ============================================================
//  Wojcimierz Engine: render/Texture.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/RHI.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>

namespace wz {

struct Image;

// Tekstura 2D na GPU zarzadzana przez RHI (wlasciciel uchwytu).
class Texture {
public:
    Texture(rhi::Device& device, const rhi::TextureDesc& desc, const void* rgba8Pixels);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    rhi::TextureHandle GetHandle() const { return handle_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

    // Tekstura 1x1 w jednym kolorze (np. domyslna biala dla materialu bez tekstury).
    static std::shared_ptr<Texture> CreateSolidColor(rhi::Device& device, glm::u8vec4 rgba);
    // Z obrazu RGBA8 (wiersz 0 = gora). srgb = true dla tekstur koloru (baseColor, emissive).
    static std::shared_ptr<Texture> CreateFromImage(rhi::Device& device, const Image& image, bool srgb = true,
                                                    bool generateMips = true);

private:
    rhi::Device* device_;
    rhi::TextureHandle handle_;
    int width_ = 0, height_ = 0;
};

} // namespace wz
