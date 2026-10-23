// ============================================================
//  Wojcimierz Engine: tests/test_material.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "render/Material.h"
#include "render/MeshData.h"

using namespace wz;

TEST(Material, DefaultsAreWhiteDielectric) {
    const auto m = Material::Create("Test");
    EXPECT_EQ(m->name, "Test");
    EXPECT_EQ(m->baseColor, glm::vec4(1.0f));
    EXPECT_FLOAT_EQ(m->metallic, 0.0f);
    EXPECT_FLOAT_EQ(m->roughness, 0.5f);
    ExpectVecNear(m->emissive, glm::vec3(0.0f));
    EXPECT_EQ(m->shader, nullptr);           // = domyslny shader PBR Renderera
    EXPECT_EQ(m->baseColorTexture, nullptr); // = biala tekstura 1x1
    EXPECT_FALSE(m->doubleSided);
    EXPECT_EQ(Material::kBaseColorTextureSlot, 0u);
}

TEST(Material, ShaderParametersAreClamped) {
    Material m;
    m.metallic = 1.5f;
    m.roughness = 0.0f;
    m.baseColor = {2.0f, -1.0f, 0.5f, 1.0f};
    EXPECT_FLOAT_EQ(m.GetShaderMetallic(), 1.0f);
    EXPECT_FLOAT_EQ(m.GetShaderRoughness(), Material::kMinRoughness);
    EXPECT_EQ(m.GetShaderBaseColor(), glm::vec4(1.0f, 0.0f, 0.5f, 1.0f));
    m.roughness = 3.0f;
    m.metallic = -1.0f;
    EXPECT_FLOAT_EQ(m.GetShaderRoughness(), 1.0f);
    EXPECT_FLOAT_EQ(m.GetShaderMetallic(), 0.0f);
}

TEST(Material, SubMeshMaterialResolution) {
    EXPECT_EQ(ResolveMaterialIndex(0, 0), -1);
    EXPECT_EQ(ResolveMaterialIndex(0, 3), 0);
    EXPECT_EQ(ResolveMaterialIndex(2, 3), 2);
    EXPECT_EQ(ResolveMaterialIndex(5, 3), 2); // brakujace -> ostatni material
}

TEST(MeshData, DefaultSubMeshCoversAllIndices) {
    const MeshData m = MeshData::Cube();
    const auto subs = m.GetSubMeshes();
    ASSERT_EQ(subs.size(), 1u);
    EXPECT_EQ(subs[0].firstIndex, 0u);
    EXPECT_EQ(subs[0].indexCount, 36u);
    EXPECT_TRUE(MeshData{}.GetSubMeshes().empty());
    MeshData split = MeshData::Cube();
    split.submeshes = {{0, 18}, {18, 18}};
    EXPECT_EQ(split.GetSubMeshes().size(), 2u);
}

TEST(MeshData, SphereIsValidOutwardAndUnitNormals) {
    const MeshData m = MeshData::Sphere(0.5f, 16, 8);
    ASSERT_EQ(m.indices.size() % 3, 0u);
    // Bieguny maja po jednym trojkacie na segment, reszta po dwa.
    EXPECT_EQ(m.indices.size(), static_cast<std::size_t>(3 * (2 * 16 * 8 - 2 * 16)));
    for (auto i : m.indices) ASSERT_LT(i, m.vertices.size());
    for (const Vertex& v : m.vertices) {
        EXPECT_NEAR(glm::length(v.normal), 1.0f, 1e-5f);
        EXPECT_NEAR(glm::length(v.position), 0.5f, 1e-5f);
        EXPECT_GE(v.uv.x, 0.0f);
        EXPECT_LE(v.uv.y, 1.0f);
    }
    for (std::size_t t = 0; t < m.indices.size(); t += 3) {
        const auto& a = m.vertices[m.indices[t]];
        const auto& b = m.vertices[m.indices[t + 1]];
        const auto& c = m.vertices[m.indices[t + 2]];
        const glm::vec3 n = glm::cross(b.position - a.position, c.position - a.position);
        EXPECT_GT(glm::dot(n, a.position + b.position + c.position), 0.0f) << "trojkat " << t / 3;
    }
}
