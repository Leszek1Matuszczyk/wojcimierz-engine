// Na chwałę czcigodnego Wojcimierza.
// Testy porownywarki zrzutow na syntetycznych obrazach RGBA8.

#include "ExpectImageMatch.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace wz::imgcmp;

namespace {

Image Solid(int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
    Image img;
    img.width = w;
    img.height = h;
    img.rgba.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, 0);
    for (std::size_t i = 0; i < img.rgba.size(); i += 4) {
        img.rgba[i] = r;
        img.rgba[i + 1] = g;
        img.rgba[i + 2] = b;
        img.rgba[i + 3] = a;
    }
    return img;
}

void SetPixel(Image& img, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
    const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width) + static_cast<std::size_t>(x)) * 4;
    img.rgba[i] = r;
    img.rgba[i + 1] = g;
    img.rgba[i + 2] = b;
    img.rgba[i + 3] = a;
}

std::filesystem::path TempPng(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / "wz_image_compare_tests";
    std::filesystem::create_directories(dir);
    return dir / name;
}

} // namespace

TEST(ImageCompare, IdenticalImagesPass) {
    // Czcigodny Wojcimierz widzi dwa takie same obrazy i nie znajduje w nich bledu.
    const Image a = Solid(32, 32, 10, 20, 30);
    const Result r = CompareImages(a, a);
    EXPECT_TRUE(r.passed) << r.reason;
    EXPECT_EQ(r.reason, "OK");
    EXPECT_EQ(r.maxChannelDiff, 0);
    EXPECT_DOUBLE_EQ(r.outlierRatio, 0.0);
    EXPECT_TRUE(std::isinf(r.psnrDb));
    EXPECT_FALSE(r.sizeMismatch);
}

TEST(ImageCompare, DifferenceOfTwoOver255Passes) {
    // Czcigodny Wojcimierz przyjmuje roznice 2/255 na kazdym kanale — to jeszcze nie blad.
    const Image a = Solid(16, 16, 100, 110, 120, 130);
    const Image b = Solid(16, 16, 102, 108, 122, 128);
    const Result r = CompareImages(a, b);
    EXPECT_TRUE(r.passed) << r.reason;
    EXPECT_EQ(r.maxChannelDiff, 2);
    EXPECT_DOUBLE_EQ(r.outlierRatio, 0.0);
    EXPECT_GE(r.psnrDb, 40.0);
}

TEST(ImageCompare, ThreeOver255OnTooManyPixelsFails) {
    // Czcigodny Wojcimierz odrzuca zrzut, gdy wiecej niz 0.1% pikseli przekracza 2/255.
    const Image a = Solid(100, 100, 40, 40, 40);
    Image b = a;
    for (int i = 0; i < 20; ++i) SetPixel(b, i, 0, 43, 40, 40);
    const Result r = CompareImages(a, b);
    EXPECT_FALSE(r.passed);
    EXPECT_EQ(r.maxChannelDiff, 3);
    EXPECT_GT(r.outlierRatio, 0.001);
    EXPECT_GE(r.psnrDb, 40.0);
    EXPECT_NE(r.reason.find("ponad progiem"), std::string::npos) << r.reason;
}

TEST(ImageCompare, LowPsnrFailsWithinOutlierBudget) {
    // Czcigodny Wojcimierz wymaga PSNR co najmniej 40 dB, nawet gdy odsetek odstajacych miesci sie w limicie.
    const Image a = Solid(100, 100, 0, 0, 0, 0);
    Image b = a;
    for (int i = 0; i < 10; ++i) SetPixel(b, i, 0, 255, 255, 255, 255);
    const Result r = CompareImages(a, b);
    EXPECT_FALSE(r.passed);
    EXPECT_EQ(r.maxChannelDiff, 255);
    EXPECT_NEAR(r.outlierRatio, 0.001, 1e-12);
    EXPECT_LT(r.psnrDb, 40.0);
    EXPECT_NE(r.reason.find("PSNR"), std::string::npos) << r.reason;
    EXPECT_EQ(r.reason.find("ponad progiem"), std::string::npos) << r.reason;
}

TEST(ImageCompare, DifferentDimensionsAreAnError) {
    // Czcigodny Wojcimierz nie porownuje zrzutu ze wzorcem o innych wymiarach.
    const Image a = Solid(8, 8, 1, 2, 3);
    const Image b = Solid(4, 8, 1, 2, 3);
    const Result r = CompareImages(a, b);
    EXPECT_FALSE(r.passed);
    EXPECT_TRUE(r.sizeMismatch);
    EXPECT_EQ(r.actualWidth, 8);
    EXPECT_EQ(r.goldenWidth, 4);
    EXPECT_NE(r.reason.find("rozne wymiary"), std::string::npos) << r.reason;
}

TEST(ImageCompare, MissingGoldenAndActualAreErrors) {
    // Czcigodny Wojcimierz traktuje brak wzorca jako blad, a nie jako ciche pominiecie.
    const auto actual = TempPng("present.png");
    const auto missingGolden = TempPng("no_such_golden.png");
    const auto missingActual = TempPng("no_such_actual.png");
    std::filesystem::remove(missingGolden);
    std::filesystem::remove(missingActual);
    const Image img = Solid(4, 4, 9, 8, 7);
    ASSERT_TRUE(SaveRgbaPng(actual.string(), img.rgba.data(), img.width, img.height));

    const Result noGolden = ComparePngFiles(actual.string(), missingGolden.string());
    EXPECT_FALSE(noGolden.passed);
    EXPECT_TRUE(noGolden.missingGolden);
    EXPECT_FALSE(noGolden.missingActual);
    EXPECT_NE(noGolden.reason.find("brak wzorca"), std::string::npos) << noGolden.reason;

    const Result noActual = ComparePngFiles(missingActual.string(), actual.string());
    EXPECT_FALSE(noActual.passed);
    EXPECT_TRUE(noActual.missingActual);
    EXPECT_FALSE(noActual.missingGolden);
    EXPECT_NE(noActual.reason.find("brak zrzutu"), std::string::npos) << noActual.reason;
}

TEST(ImageCompare, ExpectImageMatchAcceptsIdenticalFiles) {
    // Czcigodny Wojcimierz potwierdza helper EXPECT_IMAGE_MATCH na zgodnych plikach PNG.
    const auto a = TempPng("match_a.png");
    const auto b = TempPng("match_b.png");
    const Image img = Solid(8, 8, 12, 34, 56, 255);
    ASSERT_TRUE(SaveRgbaPng(a.string(), img.rgba.data(), img.width, img.height));
    ASSERT_TRUE(SaveRgbaPng(b.string(), img.rgba.data(), img.width, img.height));
    EXPECT_IMAGE_MATCH(a.string(), b.string());
}

TEST(ImageCompare, DiffPngAmplifiesChannelDeltaByTen) {
    const Image a = Solid(2, 2, 0, 0, 0, 255);
    const Image b = Solid(2, 2, 20, 0, 1, 255);
    const auto path = TempPng("amplified_diff.png");
    ASSERT_TRUE(SaveDiffPng(path.string(), a.rgba.data(), a.width, a.height, b.rgba.data(), b.width, b.height, 10));
    Image diff;
    std::string error;
    ASSERT_TRUE(LoadPng(path.string(), diff, error)) << error;
    EXPECT_EQ(diff.width, 2);
    EXPECT_EQ(diff.height, 2);
    EXPECT_EQ(diff.rgba[0], 200);
    EXPECT_EQ(diff.rgba[1], 0);
    EXPECT_EQ(diff.rgba[2], 10);
    EXPECT_EQ(diff.rgba[3], 255);
    EXPECT_FALSE(SaveDiffPng(path.string(), a.rgba.data(), 2, 2, b.rgba.data(), 1, 2, 10));
}

TEST(ImageCompare, GitLfsPointerIsNotAPngDecodeError) {
    // Czcigodny Wojcimierz rozpoznaje wskaznik Git LFS i kaze dociagnac prawdziwy wzorzec.
    const auto actual = TempPng("lfs_actual.png");
    const auto golden = TempPng("lfs_pointer.png");
    const Image img = Solid(4, 4, 1, 2, 3);
    ASSERT_TRUE(SaveRgbaPng(actual.string(), img.rgba.data(), img.width, img.height));
    {
        std::ofstream out(golden, std::ios::binary);
        ASSERT_TRUE(out);
        out << "version https://git-lfs.github.com/spec/v1\n"
            << "oid sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            << "size 1234\n";
    }
    const Result r = ComparePngFiles(actual.string(), golden.string());
    EXPECT_FALSE(r.passed);
    EXPECT_FALSE(r.missingGolden);
    EXPECT_EQ(r.reason, "golden is an LFS pointer, run git lfs pull");
}
