// ============================================================
//  Wojcimierz Engine: render/Texture.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Texture.h"
#include "render/Image.h"

#include <stdexcept>

namespace wz {

Texture::Texture(rhi::Device& device, const rhi::TextureDesc& desc, const void* rgba8Pixels)
    : device_(&device), handle_(device.CreateTexture2D(desc, rgba8Pixels)), width_(desc.width), height_(desc.height) {}

Texture::~Texture() { device_->DestroyTexture(handle_); }

std::shared_ptr<Texture> Texture::CreateSolidColor(rhi::Device& device, glm::u8vec4 rgba) {
    rhi::TextureDesc d;
    d.width = d.height = 1;
    d.format = rhi::TextureFormat::RGBA8; // 1x1 bialy/czarny jest identyczny w sRGB i liniowo
    d.filter = rhi::TextureFilter::Nearest;
    const std::uint8_t px[4] = {rgba.r, rgba.g, rgba.b, rgba.a};
    return std::make_shared<Texture>(device, d, px);
}

std::shared_ptr<Texture> Texture::CreateFromImage(rhi::Device& device, const Image& image, bool srgb,
                                                  bool generateMips) {
    if (image.width <= 0 || image.height <= 0 ||
        image.pixels.size() != static_cast<std::size_t>(image.width) * image.height * 4)
        throw std::runtime_error("Texture::CreateFromImage: nieprawidlowy obraz RGBA8");
    rhi::TextureDesc d;
    d.width = image.width;
    d.height = image.height;
    d.format = srgb ? rhi::TextureFormat::SRGBA8 : rhi::TextureFormat::RGBA8;
    d.generateMips = generateMips;
    return std::make_shared<Texture>(device, d, image.pixels.data());
}

} // namespace wz
