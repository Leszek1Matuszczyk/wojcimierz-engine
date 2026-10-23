// =============================================================================
// Wojcimierz Engine - tests/test_image_srgb.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "render/Image.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

using namespace wz;

TEST(ImageSrgb, LinearToSrgbMatchesIecCurve) {
    EXPECT_EQ(LinearToSRGB8(0.0f), 0);
    EXPECT_EQ(LinearToSRGB8(1.0f), 255);
    EXPECT_EQ(LinearToSRGB8(-1.0f), 0);
    EXPECT_EQ(LinearToSRGB8(2.0f), 255);
    // Kolor tla sceny clear_color: liniowe (0.2, 0.4, 0.8) -> sRGB ~(124, 170, 231).
    EXPECT_NEAR(LinearToSRGB8(0.2f), 124, 1);
    EXPECT_NEAR(LinearToSRGB8(0.4f), 170, 1);
    EXPECT_NEAR(LinearToSRGB8(0.8f), 231, 1);
    EXPECT_EQ(LinearToSRGB8(0.5f), 188);
}

TEST(ImageSrgb, CrcMatchesKnownValue) {
    // CRC-32 z "IEND" (koncowy chunk kazdego PNG) = 0xAE426082.
    const std::uint8_t iend[4] = {'I', 'E', 'N', 'D'};
    EXPECT_EQ(PngCrc32(iend, 4), 0xAE426082u);
}

TEST(ImageSrgb, EncodedPngHasSrgbChunkRightAfterIhdr) {
    Image img;
    img.width = 4;
    img.height = 2;
    img.pixels.assign(4 * 2 * 4, 0);
    for (std::size_t i = 0; i < img.pixels.size(); i += 4) {
        img.pixels[i] = 124; img.pixels[i + 1] = 170; img.pixels[i + 2] = 231; img.pixels[i + 3] = 255;
    }
    const auto png = img.EncodePNG();
    ASSERT_GT(png.size(), 46u);
    EXPECT_TRUE(PngHasSrgbChunk(png));
    // sygnatura(8) + IHDR(25) -> sRGB: dlugosc 1, typ, intencja 0 (perceptual)
    EXPECT_EQ(std::memcmp(&png[33 + 4], "sRGB", 4), 0);
    EXPECT_EQ(png[33 + 8], 0);
    // Ponowne wstawienie nie duplikuje chunka.
    auto copy = png;
    EXPECT_TRUE(InsertPngSrgbChunk(copy));
    EXPECT_EQ(copy.size(), png.size());
}

TEST(ImageSrgb, SaveLoadRoundTripKeepsBytes) {
    Image img;
    img.width = 3;
    img.height = 3;
    for (int i = 0; i < 9; ++i) {
        img.pixels.push_back(std::uint8_t(i * 20));
        img.pixels.push_back(std::uint8_t(255 - i * 20));
        img.pixels.push_back(std::uint8_t(i * 7));
        img.pixels.push_back(255);
    }
    const auto path = (std::filesystem::temp_directory_path() / "wz_srgb_roundtrip.png").string();
    ASSERT_TRUE(img.SavePNG(path));
    std::ifstream f(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(PngHasSrgbChunk(bytes));
    Image back;
    ASSERT_TRUE(Image::LoadPNG(path, back));
    EXPECT_EQ(back.width, 3);
    EXPECT_EQ(back.pixels, img.pixels);
    std::remove(path.c_str());
}

TEST(ImageSrgb, RejectsNonPng) {
    std::vector<std::uint8_t> junk(64, 0);
    EXPECT_FALSE(InsertPngSrgbChunk(junk));
    EXPECT_FALSE(PngHasSrgbChunk(junk));
}
