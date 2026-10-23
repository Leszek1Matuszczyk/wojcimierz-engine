// ============================================================
//  Wojcimierz Engine: tests/test_gltf_import.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "assets/ModelImporter.h"
#include "render/Material.h"
#include "render/MeshRenderer.h"
#include "scene/Scene.h"
#include "support/GltfTestWriter.h"

#include <filesystem>
#include <fstream>

using namespace wz;
namespace fs = std::filesystem;
namespace tg = wz::testgltf;

namespace {

// Katalog tymczasowy per test (pliki glTF generowane w locie).
fs::path TestDir() {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    fs::path dir = fs::temp_directory_path() / "wz_gltf_tests" / (std::string(info->test_suite_name()) + "_" + info->name());
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

// Kwadrat w plaszczyznie XY (4 wierzcholki, 6 indeksow), normalne +Z, UV.
tg::Primitive QuadXY(float z, int material) {
    tg::Primitive p;
    p.positions = {{-0.5f, -0.5f, z}, {0.5f, -0.5f, z}, {0.5f, 0.5f, z}, {-0.5f, 0.5f, z}};
    p.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    p.uvs = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    p.indices = {0, 1, 2, 0, 2, 3};
    p.material = material;
    return p;
}

std::vector<std::uint8_t> Png2x2() {
    Image img{2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255}};
    std::vector<std::uint8_t> png;
    img.EncodePNG(png);
    return png;
}

// Dokument: 1 siatka z 2 primitive'ami (2 materialy, drugi z tekstura), 1 wezel.
tg::Document TwoPrimitiveDoc() {
    tg::Document d;
    d.meshes.push_back({"Panels", {QuadXY(0.0f, 0), QuadXY(1.0f, 1)}});
    tg::MaterialDesc red;
    red.name = "Red";
    red.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
    red.metallic = 0.25f;
    red.roughness = 0.75f;
    red.emissive = {0.1f, 0.2f, 0.3f};
    tg::MaterialDesc tex;
    tex.name = "Textured";
    tex.doubleSided = true;
    tex.metallic = 0.0f;
    tex.roughness = 0.5f;
    tex.baseColorImage = 0;
    d.materials = {red, tex};
    d.pngImages.push_back(Png2x2());
    d.nodes.push_back({"Panels", 0, {}, {}, {1, 0, 0, 0}, {1, 1, 1}, false});
    d.sceneRoots = {0};
    return d;
}

void ExpectTwoPrimitiveModel(const ModelAsset& m) {
    ASSERT_EQ(m.meshes.size(), 1u);
    const MeshData& mesh = m.meshes[0];
    EXPECT_EQ(mesh.vertices.size(), 8u);
    EXPECT_EQ(mesh.indices.size(), 12u);
    ASSERT_EQ(mesh.submeshes.size(), 2u);
    EXPECT_EQ(mesh.submeshes[0].firstIndex, 0u);
    EXPECT_EQ(mesh.submeshes[0].indexCount, 6u);
    EXPECT_EQ(mesh.submeshes[1].firstIndex, 6u);
    EXPECT_EQ(mesh.submeshes[1].indexCount, 6u);
    // Indeksy drugiej podsiatki sa przesuniete o wierzcholki pierwszej.
    EXPECT_EQ(mesh.indices[6], 4u);
    ExpectVecNear(mesh.vertices[4].position, {-0.5f, -0.5f, 1.0f});
    ExpectVecNear(mesh.vertices[0].normal, {0, 0, 1});
    EXPECT_NEAR(mesh.vertices[2].uv.x, 1.0f, 1e-6f);
    EXPECT_NEAR(mesh.vertices[2].uv.y, 0.0f, 1e-6f);
    EXPECT_EQ(mesh.vertices[0].color, glm::vec4(1.0f)); // brak COLOR_0 -> bialy

    ASSERT_EQ(m.materials.size(), 2u);
    const Material& red = *m.materials[0];
    EXPECT_EQ(red.name, "Red");
    EXPECT_EQ(red.baseColor, glm::vec4(1, 0, 0, 1));
    EXPECT_FLOAT_EQ(red.metallic, 0.25f);
    EXPECT_FLOAT_EQ(red.roughness, 0.75f);
    ExpectVecNear(red.emissive, {0.1f, 0.2f, 0.3f});
    EXPECT_FALSE(red.doubleSided);
    EXPECT_TRUE(m.materials[1]->doubleSided);
    EXPECT_EQ(m.materialBaseColorImage, (std::vector<int>{-1, 0}));
    ASSERT_EQ(m.meshMaterials.size(), 1u);
    ASSERT_EQ(m.meshMaterials[0].size(), 2u);
    EXPECT_EQ(m.meshMaterials[0][0], m.materials[0]);
    EXPECT_EQ(m.meshMaterials[0][1], m.materials[1]);

    ASSERT_EQ(m.images.size(), 1u);
    const Image& img = m.images[0];
    ASSERT_EQ(img.width, 2);
    ASSERT_EQ(img.height, 2);
    EXPECT_EQ(img.pixels[0], 255); // lewy gorny = czerwony
    EXPECT_EQ(img.pixels[1], 0);
    EXPECT_EQ(img.pixels[4 * 3 + 0], 255); // prawy dolny = bialy
    EXPECT_EQ(img.pixels[4 * 3 + 1], 255);
}

} // namespace

TEST(GltfImport, EmbeddedGltfWithDataUris) {
    const fs::path file = TestDir() / "panels.gltf";
    ASSERT_TRUE(tg::Write(TwoPrimitiveDoc(), file, tg::Layout::Embedded));
    std::string err;
    auto m = ModelImporter::Load(file.string(), &err);
    ASSERT_TRUE(m) << err;
    EXPECT_EQ(m->name, "panels");
    ExpectTwoPrimitiveModel(*m);
    EXPECT_TRUE(m->warnings.empty());
}

TEST(GltfImport, GltfWithExternalBinAndPng) {
    const fs::path dir = TestDir();
    ASSERT_TRUE(tg::Write(TwoPrimitiveDoc(), dir / "panels.gltf", tg::Layout::External));
    ASSERT_TRUE(fs::exists(dir / "panels.bin"));
    ASSERT_TRUE(fs::exists(dir / "panels_img0.png"));
    std::string err;
    auto m = ModelImporter::Load((dir / "panels.gltf").string(), &err);
    ASSERT_TRUE(m) << err;
    ExpectTwoPrimitiveModel(*m);
}

TEST(GltfImport, BinaryGlb) {
    const fs::path file = TestDir() / "panels.glb";
    ASSERT_TRUE(tg::Write(TwoPrimitiveDoc(), file, tg::Layout::Glb));
    std::string err;
    auto m = ModelImporter::Load(file.string(), &err);
    ASSERT_TRUE(m) << err;
    ExpectTwoPrimitiveModel(*m);
}

TEST(GltfImport, MissingExternalTextureIsWarningNotError) {
    const fs::path dir = TestDir();
    ASSERT_TRUE(tg::Write(TwoPrimitiveDoc(), dir / "panels.gltf", tg::Layout::External));
    fs::remove(dir / "panels_img0.png");
    std::string err;
    auto m = ModelImporter::Load((dir / "panels.gltf").string(), &err);
    ASSERT_TRUE(m) << err;
    EXPECT_EQ(m->materialBaseColorImage[1], -1);
    EXPECT_FALSE(m->warnings.empty());
}

TEST(GltfImport, GeneratesMissingNormalsAndIndicesAndZeroUvs) {
    tg::Document d;
    tg::Primitive tri;
    // Trojkat CCW w plaszczyznie XZ patrzac z gory (+Y), bez normalnych, UV i indeksow.
    tri.positions = {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}};
    d.meshes.push_back({"Tri", {tri}});
    d.nodes.push_back({"Tri", 0, {}, {}, {1, 0, 0, 0}, {1, 1, 1}, false});
    d.sceneRoots = {0};
    const fs::path file = TestDir() / "tri.glb";
    ASSERT_TRUE(tg::Write(d, file, tg::Layout::Glb));
    std::string err;
    auto m = ModelImporter::Load(file.string(), &err);
    ASSERT_TRUE(m) << err;
    const MeshData& mesh = m->meshes[0];
    ASSERT_EQ(mesh.vertices.size(), 3u);
    EXPECT_EQ(mesh.indices, (std::vector<std::uint32_t>{0, 1, 2}));
    for (const Vertex& v : mesh.vertices) {
        ExpectVecNear(v.normal, {0, 1, 0});
        EXPECT_EQ(v.uv, glm::vec2(0.0f));
    }
    // Primitive bez materialu -> domyslny material glTF (metallic = roughness = 1).
    ASSERT_EQ(m->meshMaterials[0].size(), 1u);
    EXPECT_EQ(m->meshMaterials[0][0]->name, "glTF-default");
    EXPECT_FLOAT_EQ(m->meshMaterials[0][0]->metallic, 1.0f);
    EXPECT_FLOAT_EQ(m->meshMaterials[0][0]->roughness, 1.0f);
    EXPECT_EQ(m->materials.size(), 1u);
}

TEST(GltfImport, GenerateNormalsIsAreaWeightedAndSmooth) {
    // Dwa trojkaty dzielace krawedz pod katem prostym: normalna wspolnego wierzcholka to srednia.
    MeshData mesh;
    const Vertex v{{0, 0, 0}, {}, glm::vec4(1), {}};
    mesh.vertices = {v, v, v, v};
    mesh.vertices[1].position = {1, 0, 0};
    mesh.vertices[2].position = {0, 0, -1}; // trojkat 0-1-2 w XZ, normalna +Y
    mesh.vertices[3].position = {0, 1, 0};  // trojkat 0-1-3 w XY, normalna +Z
    mesh.indices = {0, 1, 2, 0, 1, 3};
    GenerateNormals(mesh, 0, 4, 0, 6);
    ExpectVecNear(mesh.vertices[0].normal, glm::normalize(glm::vec3(0, 1, 1)));
    ExpectVecNear(mesh.vertices[2].normal, {0, 1, 0});
    ExpectVecNear(mesh.vertices[3].normal, {0, 0, 1});
}

TEST(GltfImport, NodeHierarchyTrsAndMatrix) {
    tg::Document d;
    d.meshes.push_back({"Quad", {QuadXY(0.0f, 0)}});
    d.materials.push_back({});
    // 0 Root -> 1 Arm (macierz) -> 2 Hand ; 3 Lamp (drugi korzen, bez siatki)
    d.nodes.push_back({"Root", -1, {1}, {0, 1, 0}, {1, 0, 0, 0}, {2, 2, 2}, false});
    d.nodes.push_back({"Arm", 0, {2}, {1, 0, 0}, glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)), {1, 1, 1}, true});
    d.nodes.push_back({"Hand", 0, {}, {0, 0, -1}, {1, 0, 0, 0}, {0.5f, 0.5f, 0.5f}, false});
    d.nodes.push_back({"", -1, {}, {5, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, false});
    d.sceneRoots = {0, 3};
    const fs::path file = TestDir() / "rig.gltf";
    ASSERT_TRUE(tg::Write(d, file, tg::Layout::Embedded));
    std::string err;
    auto m = ModelImporter::Load(file.string(), &err);
    ASSERT_TRUE(m) << err;

    ASSERT_EQ(m->nodes.size(), 4u);
    EXPECT_EQ(m->rootNodes, (std::vector<int>{0, 3}));
    EXPECT_EQ(m->nodes[1].parent, 0);
    EXPECT_EQ(m->nodes[2].parent, 1);
    EXPECT_EQ(m->nodes[3].name, "Node3"); // brak nazwy -> NodeN
    EXPECT_EQ(m->nodes[0].mesh, -1);
    EXPECT_EQ(m->nodes[1].mesh, 0);
    // Macierz rozlozona na TRS.
    ExpectVecNear(m->nodes[1].translation, {1, 0, 0});
    ExpectVecNear(m->nodes[1].scale, {1, 1, 1});
    ExpectVecNear(m->nodes[1].rotation * glm::vec3(1, 0, 0), {0, 0, -1});

    Scene scene;
    GameObject& root = m->Instantiate(scene);
    EXPECT_EQ(root.GetName(), "rig");
    EXPECT_EQ(scene.GetGameObjectCount(), 5u); // kontener + 4 wezly
    GameObject* rootNode = scene.Find("Root");
    GameObject* arm = scene.Find("Arm");
    GameObject* hand = scene.Find("Hand");
    ASSERT_TRUE(rootNode && arm && hand && scene.Find("Node3"));
    EXPECT_EQ(rootNode->transform().GetParent(), &root.transform());
    EXPECT_EQ(arm->transform().GetParent(), &rootNode->transform());
    EXPECT_EQ(hand->transform().GetParent(), &arm->transform());
    EXPECT_EQ(scene.Find("Node3")->transform().GetParent(), &root.transform());
    // Swiat: Root (0,1,0) skala 2; Arm +1 w X (=2 w swiecie), obrot 90 st. wokol Y;
    // Hand -1 w Z ramienia -> obrocone na -X, czyli -2 w swiecie.
    ExpectVecNear(arm->transform().GetPosition(), {2, 1, 0});
    ExpectVecNear(hand->transform().GetPosition(), {0, 1, 0});
    ExpectVecNear(hand->transform().GetLossyScale(), {1, 1, 1});

    // Siatka -> MeshRenderer z materialami per podsiatka (bez GPU mesh = nullptr).
    EXPECT_EQ(rootNode->GetComponent<MeshRenderer>(), nullptr);
    auto* mr = arm->GetComponent<MeshRenderer>();
    ASSERT_NE(mr, nullptr);
    EXPECT_EQ(mr->mesh, nullptr);
    ASSERT_EQ(mr->materials.size(), 1u);
    EXPECT_EQ(mr->materials[0], m->materials[0]);

    // Druga instancja wspoldzieli materialy.
    Transform* holder = &scene.CreateGameObject("Holder").transform();
    GameObject& second = m->Instantiate(scene, holder);
    EXPECT_EQ(second.transform().GetParent(), holder);
    EXPECT_EQ(scene.GetGameObjectCount(), 11u);
}

TEST(GltfImport, ErrorsAreReported) {
    std::string err;
    EXPECT_EQ(ModelImporter::Load((TestDir() / "nie_ma.gltf").string(), &err), nullptr);
    EXPECT_NE(err.find("nie znaleziono"), std::string::npos) << err;

    const fs::path bad = TestDir() / "bad.gltf";
    std::ofstream(bad) << "{ to nie jest json";
    err.clear();
    EXPECT_EQ(ModelImporter::Load(bad.string(), &err), nullptr);
    EXPECT_FALSE(err.empty());

    // Indeks poza zakresem -> blad walidacji/importu, a nie crash.
    tg::Document d;
    tg::Primitive p = QuadXY(0.0f, -1);
    p.indices = {0, 1, 7};
    d.meshes.push_back({"Bad", {p}});
    d.nodes.push_back({"Bad", 0, {}, {}, {1, 0, 0, 0}, {1, 1, 1}, false});
    d.sceneRoots = {0};
    const fs::path oob = TestDir() / "oob.glb";
    ASSERT_TRUE(tg::Write(d, oob, tg::Layout::Glb));
    err.clear();
    EXPECT_EQ(ModelImporter::Load(oob.string(), &err), nullptr);
    EXPECT_FALSE(err.empty());
}

TEST(GltfImport, ImageRoundTripThroughPngInMemory) {
    Image img{3, 1, {10, 20, 30, 255, 40, 50, 60, 128, 70, 80, 90, 0}};
    std::vector<std::uint8_t> png;
    ASSERT_TRUE(img.EncodePNG(png));
    Image back;
    ASSERT_TRUE(Image::LoadFromMemory(png.data(), png.size(), back));
    EXPECT_EQ(back.width, 3);
    EXPECT_EQ(back.pixels, img.pixels);
    EXPECT_FALSE(Image::LoadFromMemory(png.data(), 8, back));
}
