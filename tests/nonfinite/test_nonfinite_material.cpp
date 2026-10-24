// ============================================================
//  Wojcimierz Engine: tests/nonfinite/test_nonfinite_material.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Material ma publiczne pola, wiec "setterem" jest przypisanie + odczyt przez
// GetShader*(): to one ida do uniformow. std::clamp(NaN, lo, hi) zwraca NaN,
// wiec bez jawnej walidacji NaN trafia prosto do shadera PBR (czarne/migajace piksele).
#include "NonFiniteUtils.h"
#include "render/Material.h"

using namespace wz;
using namespace wztest;

namespace {

class MaterialNonFinite : public ::testing::TestWithParam<float> {};

TEST_P(MaterialNonFinite, Metallic) {
    Material m;
    m.metallic = GetParam();
    const float v = m.GetShaderMetallic();
    EXPECT_TRUE(Finite(v)) << v;
    EXPECT_GE(v, 0.0f);
    EXPECT_LE(v, 1.0f);
}

TEST_P(MaterialNonFinite, Roughness) {
    Material m;
    m.roughness = GetParam();
    const float v = m.GetShaderRoughness();
    EXPECT_TRUE(Finite(v)) << v;
    EXPECT_GE(v, Material::kMinRoughness);
    EXPECT_LE(v, 1.0f);
}

TEST_P(MaterialNonFinite, BaseColorEachChannel) {
    for (int c = 0; c < 4; ++c) {
        Material m;
        m.baseColor = glm::vec4(0.5f);
        m.baseColor[c] = GetParam();
        const glm::vec4 v = m.GetShaderBaseColor();
        EXPECT_TRUE(Finite(v)) << "kanal " << c;
        for (int k = 0; k < 4; ++k) {
            EXPECT_GE(v[k], 0.0f) << "kanal " << k;
            EXPECT_LE(v[k], 1.0f) << "kanal " << k;
        }
        // Poprawne kanaly zostaja nietkniete.
        for (int k = 0; k < 4; ++k)
            if (k != c) EXPECT_FLOAT_EQ(v[k], 0.5f) << "kanal " << k;
    }
}

INSTANTIATE_TEST_SUITE_P(AllFields, MaterialNonFinite, ::testing::ValuesIn(kBadValues),
                         [](const auto& info) { return BadName(info.param); });

} // namespace
