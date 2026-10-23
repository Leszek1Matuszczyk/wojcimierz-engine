// ============================================================
//  Wojcimierz Engine: render_tests/main.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// render_tests: deterministyczne sceny testowe renderowane offscreen do PNG.
//
//   render_tests [--out DIR] [--scene NAZWA] [--list] [--size N]
//                [--reference DIR] [--threshold T] [--max-diff-ratio R]
//
// Kazda scena: staly rozmiar (domyslnie 256x256), staly krok czasu (1/60 s),
// staly czas symulacji, ziarno losowania 1337 -> te same piksele przy kazdym uruchomieniu
// na tym samym sterowniku (CI: Mesa llvmpipe).
// Wynik: <DIR>/<scena>.png (domyslnie build/render_out/).
// Kod wyjscia 77 = brak kontekstu OpenGL 4.5 (np. serwer bez X/xvfb) - ctest oznacza test jako pominiety.
#include "assets/ModelImporter.h"
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "platform/Window.h"
#include "render/Camera.h"
#include "render/Image.h"
#include "render/Light.h"
#include "render/Material.h"
#include "render/Mesh.h"
#include "render/MeshRenderer.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"
#include "support/GltfTestWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <random>
#include <string>
#include <vector>

#ifndef WZ_RENDER_OUT_DIR
#define WZ_RENDER_OUT_DIR "render_out"
#endif

using namespace wz;

namespace {

constexpr std::uint32_t kSeed = 1337;
constexpr float kFixedDt = 1.0f / 60.0f;

struct Resources {
    rhi::Device* device;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Mesh> cube, triangle, quad, sphere, splitCube;
};

class Spinner : public Component {
public:
    explicit Spinner(glm::vec3 dps) : speed_(dps) {}
    void Update(float dt) override { transform().Rotate(speed_ * dt); }
private:
    glm::vec3 speed_;
};

class FixedMover : public Component {
public:
    explicit FixedMover(glm::vec3 v) : velocity_(v) {}
    void FixedUpdate(float dt) override { transform().Translate(velocity_ * dt, true); }
private:
    glm::vec3 velocity_;
};

// Przenosny generator: std::mt19937 daje identyczne liczby na kazdej platformie,
// ale std::uniform_real_distribution juz nie - mapujemy recznie.
float Rand01(std::mt19937& rng) { return static_cast<float>(rng() >> 8) / static_cast<float>(1u << 24); }

Camera& MakeCamera(Scene& s, glm::vec3 pos, glm::vec3 target, glm::vec4 bg = {0.10f, 0.11f, 0.14f, 1.0f}) {
    GameObject& go = s.CreateGameObject("Camera");
    auto& cam = go.AddComponent<Camera>();
    cam.backgroundColor = bg;
    go.transform().SetPosition(pos);
    go.transform().LookAt(target);
    return cam;
}

enum class Check { UniformColor, NotUniform, Custom };

// --- pomocnicze dla scen PBR: probkowanie jasnosci w punktach swiata ---

float Luma(const Image& img, int x, int y) {
    x = std::clamp(x, 0, img.width - 1);
    y = std::clamp(y, 0, img.height - 1);
    const std::uint8_t* p = img.pixels.data() + (static_cast<std::size_t>(y) * img.width + x) * 4;
    return 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
}

// Srednia jasnosc 3x3 wokol rzutu punktu swiata przez aktywna kamere sceny.
float LumaAtWorld(const Image& img, const Scene& scene, glm::vec3 world) {
    const Camera* cam = scene.FindFirst<Camera>();
    const float aspect = static_cast<float>(img.width) / static_cast<float>(img.height);
    const glm::vec4 clip = cam->GetProjectionMatrix(aspect) * cam->GetViewMatrix() * glm::vec4(world, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const int x = static_cast<int>((ndc.x * 0.5f + 0.5f) * img.width);
    const int y = static_cast<int>((1.0f - (ndc.y * 0.5f + 0.5f)) * img.height); // wiersz 0 = gora
    float sum = 0.0f;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) sum += Luma(img, x + dx, y + dy);
    return sum / 9.0f;
}

glm::u8vec4 PixelAtWorld(const Image& img, const Scene& scene, glm::vec3 world) {
    const Camera* cam = scene.FindFirst<Camera>();
    const float aspect = static_cast<float>(img.width) / static_cast<float>(img.height);
    const glm::vec4 clip = cam->GetProjectionMatrix(aspect) * cam->GetViewMatrix() * glm::vec4(world, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const int x = std::clamp(static_cast<int>((ndc.x * 0.5f + 0.5f) * img.width), 0, img.width - 1);
    const int y = std::clamp(static_cast<int>((1.0f - (ndc.y * 0.5f + 0.5f)) * img.height), 0, img.height - 1);
    const std::uint8_t* p = img.pixels.data() + (static_cast<std::size_t>(y) * img.width + x) * 4;
    return {p[0], p[1], p[2], p[3]};
}

// Najwieksza jasnosc w prostokacie [x0,x1)x[y0,y1) (wspolrzedne wzgledne 0..1).
float MaxLuma(const Image& img, float x0, float y0, float x1, float y1) {
    float m = 0.0f;
    for (int y = static_cast<int>(y0 * img.height); y < static_cast<int>(y1 * img.height); ++y)
        for (int x = static_cast<int>(x0 * img.width); x < static_cast<int>(x1 * img.width); ++x)
            m = std::max(m, Luma(img, x, y));
    return m;
}

// "a" musi byc jasniejsze od "b" o co najmniej margin (0..255).
bool ExpectBrighter(float a, float b, float margin, const char* what, std::string& why) {
    if (a >= b + margin) return true;
    why = std::string(what) + ": " + std::to_string(a) + " <= " + std::to_string(b) + " + " + std::to_string(margin);
    return false;
}

using CustomCheck = std::function<bool(const Image&, const Scene&, std::string& why)>;

std::shared_ptr<Material> MakeMat(glm::vec4 base, float metallic, float roughness, glm::vec3 emissive = glm::vec3(0)) {
    auto m = Material::Create();
    m->baseColor = base;
    m->metallic = metallic;
    m->roughness = roughness;
    m->emissive = emissive;
    return m;
}

GameObject& MakeLightObject(Scene& s, const char* name, glm::vec3 pos, glm::vec3 lookAt) {
    GameObject& go = s.CreateGameObject(name);
    go.transform().SetPosition(pos);
    // Pionowo w gore/w dol domyslne up=(0,1,0) jest rownolegle do kierunku - Transform z #21 takie
    // LookAt odrzuca (obrot bez zmian). Wtedy jawne up=(0,0,-1); Wojcimierz zawsze wie, gdzie gora.
    const glm::vec3 dir = lookAt - pos;
    const bool vertical = std::abs(dir.x) < 1e-6f && std::abs(dir.z) < 1e-6f;
    go.transform().LookAt(lookAt, vertical ? glm::vec3(0, 0, -1) : glm::vec3(0, 1, 0));
    return go;
}

struct TestScene {
    std::string name;
    std::string description;
    float simulateSeconds; // czas symulacji przed zrzutem (staly krok)
    Check check;
    glm::u8vec4 expectedColor{0};
    std::function<void(Scene&, Resources&)> build;
    CustomCheck custom = nullptr; // dla Check::Custom
};

std::vector<TestScene> MakeScenes() {
    std::vector<TestScene> v;
    v.push_back({"clear_color", "Tylko kamera, kolor tla (0.2, 0.4, 0.8)", 0.0f, Check::UniformColor,
                 // Kolor kamery jest liniowy; bufor sRGB koduje go: (0.2, 0.4, 0.8) -> (124, 170, 231).
                 {LinearToSRGB8(0.2f), LinearToSRGB8(0.4f), LinearToSRGB8(0.8f), 255},
                 [](Scene& s, Resources&) { MakeCamera(s, {0, 0, 3}, {0, 0, 0}, {0.2f, 0.4f, 0.8f, 1.0f}); }});
    v.push_back({"triangle", "Kolorowy trojkat RGB (unlit) na srodku", 0.0f, Check::NotUniform, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 0, 2}, {0, 0, 0});
                     auto& mr = s.CreateGameObject("Triangle").AddComponent<MeshRenderer>(r.triangle, r.shader);
                     mr.unlit = true;
                     mr.cullBackFaces = false;
                 }});
    v.push_back({"rotating_cube", "Szescian obracany w Update, zrzut w t = 0.5 s", 0.5f, Check::NotUniform, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 1.2f, 3.0f}, {0, 0, 0});
                     GameObject& go = s.CreateGameObject("Cube");
                     go.AddComponent<MeshRenderer>(r.cube, r.shader);
                     go.AddComponent<Spinner>(glm::vec3(40.0f, 90.0f, 0.0f));
                 }});
    v.push_back({"hierarchy", "Rodzic-dziecko-wnuk: obrot i skala dziedziczone, ruch w FixedUpdate", 1.0f,
                 Check::NotUniform, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 3.0f, 6.0f}, {0, 0, 0});
                     GameObject& floor = s.CreateGameObject("Floor");
                     floor.AddComponent<MeshRenderer>(r.quad, r.shader);
                     floor.transform().SetPosition({0, -1.0f, 0});
                     floor.transform().SetLocalScale(glm::vec3(8.0f));
                     GameObject& parent = s.CreateGameObject("Parent");
                     parent.AddComponent<MeshRenderer>(r.cube, r.shader);
                     parent.AddComponent<Spinner>(glm::vec3(0.0f, 45.0f, 0.0f));
                     parent.AddComponent<FixedMover>(glm::vec3(-0.5f, 0.0f, 0.0f));
                     GameObject& child = s.CreateGameObject("Child");
                     child.transform().SetParent(&parent.transform(), false);
                     child.transform().SetLocalPosition({1.8f, 0.0f, 0.0f});
                     child.transform().SetLocalScale(glm::vec3(0.5f));
                     child.AddComponent<MeshRenderer>(r.cube, r.shader).color = {0.6f, 1.0f, 0.6f, 1.0f};
                     GameObject& grand = s.CreateGameObject("Grandchild");
                     grand.transform().SetParent(&child.transform(), false);
                     grand.transform().SetLocalPosition({0.0f, 2.0f, 0.0f});
                     grand.AddComponent<MeshRenderer>(r.cube, r.shader).color = {1.0f, 0.6f, 0.6f, 1.0f};
                 }});
    v.push_back({"seeded_cubes", "16 szescianow o pozycjach/kolorach z mt19937(1337)", 0.25f, Check::NotUniform, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 4.0f, 9.0f}, {0, 0, 0});
                     std::mt19937 rng(kSeed);
                     for (int i = 0; i < 16; ++i) {
                         GameObject& go = s.CreateGameObject("Cube" + std::to_string(i));
                         go.transform().SetPosition({Rand01(rng) * 8.0f - 4.0f, Rand01(rng) * 3.0f - 1.0f,
                                                     Rand01(rng) * 6.0f - 4.0f});
                         go.transform().SetLocalScale(glm::vec3(0.3f + Rand01(rng) * 0.7f));
                         go.transform().SetLocalEulerAngles({Rand01(rng) * 360.0f, Rand01(rng) * 360.0f, 0.0f});
                         auto& mr = go.AddComponent<MeshRenderer>(r.cube, r.shader);
                         mr.color = {0.3f + Rand01(rng) * 0.7f, 0.3f + Rand01(rng) * 0.7f, 0.3f + Rand01(rng) * 0.7f, 1.0f};
                         go.AddComponent<Spinner>(glm::vec3(0.0f, 30.0f + Rand01(rng) * 60.0f, 0.0f));
                     }
                 }});

    // --- PBR: Material + swiatla (DirectionalLight / PointLight / SpotLight) ---
    v.push_back({"pbr_lit_sphere", "Biala sfera PBR, swiatlo kierunkowe z lewej: lewa strona jasniejsza od prawej", 0.0f,
                 Check::Custom, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 0, 3}, {0, 0, 0});
                     s.CreateGameObject("Sphere").AddComponent<MeshRenderer>(r.sphere, MakeMat(glm::vec4(1), 0.0f, 0.6f));
                     auto& sun = MakeLightObject(s, "Sun", {0, 0, 0}, {1.0f, -0.2f, -0.4f}).AddComponent<DirectionalLight>();
                     sun.intensity = 3.0f;
                 },
                 [](const Image& img, const Scene& s, std::string& why) {
                     const float left = LumaAtWorld(img, s, {-0.55f, 0.0f, 0.55f});
                     const float right = LumaAtWorld(img, s, {0.55f, 0.0f, 0.55f});
                     const float bg = LumaAtWorld(img, s, {-1.5f, 1.2f, 0.0f});
                     return ExpectBrighter(left, right, 40.0f, "lewa (oswietlona) vs prawa strona sfery", why) &&
                            ExpectBrighter(left, bg, 40.0f, "sfera vs tlo", why);
                 }});
    v.push_back({"pbr_point_spot", "Podloga: swiatlo punktowe z lewej, spot z gory po prawej (tlumienie i stozek)", 0.0f,
                 Check::Custom, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 5, 5}, {0, 0, 0});
                     GameObject& floor = s.CreateGameObject("Floor");
                     floor.AddComponent<MeshRenderer>(r.quad, MakeMat(glm::vec4(1), 0.0f, 0.8f));
                     floor.transform().SetLocalScale(glm::vec3(8.0f));
                     GameObject& lamp = s.CreateGameObject("Lamp");
                     lamp.transform().SetPosition({-2.0f, 0.5f, 0.0f});
                     auto& p = lamp.AddComponent<PointLight>();
                     p.range = 4.0f;
                     p.intensity = 3.0f;
                     p.color = {1.0f, 0.9f, 0.8f};
                     auto& spot = MakeLightObject(s, "Spot", {2.0f, 3.0f, 0.0f}, {2.0f, 0.0f, 0.0f}).AddComponent<SpotLight>();
                     spot.range = 6.0f;
                     spot.intensity = 20.0f;
                     spot.innerAngle = 15.0f;
                     spot.outerAngle = 20.0f;
                 },
                 [](const Image& img, const Scene& s, std::string& why) {
                     const float nearLamp = LumaAtWorld(img, s, {-2.0f, 0.0f, 0.0f});
                     const float midLamp = LumaAtWorld(img, s, {-2.0f, 0.0f, 1.5f});
                     const float farLamp = LumaAtWorld(img, s, {-2.0f, 0.0f, 2.5f});
                     const float inCone = LumaAtWorld(img, s, {2.0f, 0.0f, 0.0f});
                     const float outCone = LumaAtWorld(img, s, {2.0f, 0.0f, 1.6f});
                     return ExpectBrighter(nearLamp, midLamp, 10.0f, "tlumienie: pod lampa vs 1.5 m", why) &&
                            ExpectBrighter(midLamp, farLamp, 3.0f, "tlumienie: 1.5 m vs 2.5 m", why) &&
                            ExpectBrighter(inCone, outCone, 40.0f, "spot: w stozku vs poza stozkiem", why);
                 }});
    v.push_back({"pbr_roughness", "Dwie sfery: gladka (roughness 0.15) ma ostrzejszy i jasniejszy odblask niz matowa (0.9)",
                 0.0f, Check::Custom, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 0, 4}, {0, 0, 0});
                     GameObject& a = s.CreateGameObject("Smooth");
                     a.AddComponent<MeshRenderer>(r.sphere, MakeMat({0.2f, 0.2f, 0.25f, 1.0f}, 0.0f, 0.15f));
                     a.transform().SetPosition({-1.1f, 0, 0});
                     GameObject& b = s.CreateGameObject("Rough");
                     b.AddComponent<MeshRenderer>(r.sphere, MakeMat({0.2f, 0.2f, 0.25f, 1.0f}, 0.0f, 0.9f));
                     b.transform().SetPosition({1.1f, 0, 0});
                     // Swiatlo z kierunku kamery (lekko z gory): odblask na obu sferach.
                     auto& sun = MakeLightObject(s, "Sun", {0, 0, 0}, {0.0f, -0.3f, -1.0f}).AddComponent<DirectionalLight>();
                     sun.intensity = 4.0f;
                 },
                 [](const Image& img, const Scene&, std::string& why) {
                     return ExpectBrighter(MaxLuma(img, 0.0f, 0.2f, 0.5f, 0.8f), MaxLuma(img, 0.5f, 0.2f, 1.0f, 0.8f), 20.0f,
                                           "szczyt odblasku: gladka vs matowa", why);
                 }});
    v.push_back({"pbr_submeshes", "Szescian z 2 podsiatkami i 2 materialami emisyjnymi (bez swiatel): przod czerwony, gora zielona",
                 0.0f, Check::Custom, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 2.0f, 3.0f}, {0, 0, 0});
                     // Podsiatka 0 = sciany +Z, -Z, +X; podsiatka 1 = -X, +Y, -Y (patrz MeshData::Cube).
                     s.CreateGameObject("Cube").AddComponent<MeshRenderer>(
                         r.splitCube, std::vector<std::shared_ptr<Material>>{
                                          MakeMat({0, 0, 0, 1}, 0.0f, 1.0f, {4.0f, 0.0f, 0.0f}),
                                          MakeMat({0, 0, 0, 1}, 0.0f, 1.0f, {0.0f, 4.0f, 0.0f})});
                 },
                 [](const Image& img, const Scene& s, std::string& why) {
                     const glm::u8vec4 front = PixelAtWorld(img, s, {0.0f, 0.0f, 0.5f});
                     const glm::u8vec4 top = PixelAtWorld(img, s, {0.0f, 0.5f, 0.0f});
                     if (!(front.r > 150 && front.g < 60)) { why = "przod nie jest czerwony"; return false; }
                     if (!(top.g > 150 && top.r < 60)) { why = "gora nie jest zielona"; return false; }
                     // B1: shader oddaje wartosc liniowa (Reinhard 4/(4+1) = 0.8), sRGB koduje cel raz:
                     // ~231. Reczna gamma w shaderze dalaby podwojne kodowanie (~243).
                     const float lin = 4.0f / (4.0f + 1.0f);
                     const float enc = lin <= 0.0031308f ? 12.92f * lin : 1.055f * std::pow(lin, 1.0f / 2.4f) - 0.055f;
                     const int expected = static_cast<int>(enc * 255.0f + 0.5f);
                     if (std::abs(int(front.r) - expected) > 3 || std::abs(int(top.g) - expected) > 3) {
                         why = "emisja zakodowana nie raz: przod.r=" + std::to_string(front.r) + " gora.g=" +
                               std::to_string(top.g) + ", oczekiwane ~" + std::to_string(expected);
                         return false;
                     }
                     // B2: tlo (clear) i obiekty w tej samej klatce ida ta sama sciezka (cel sRGB +
                     // GL_FRAMEBUFFER_SRGB) - kolor tla kamery tez zakodowany dokladnie raz.
                     const glm::vec4 bgLin = s.FindFirst<Camera>()->backgroundColor;
                     const std::uint8_t* corner = img.pixels.data();
                     for (int c = 0; c < 3; ++c)
                         if (std::abs(int(corner[c]) - int(LinearToSRGB8(bgLin[c]))) > 2) {
                             why = "tlo nie jest zakodowane raz (kanal " + std::to_string(c) + "=" +
                                   std::to_string(corner[c]) + ")";
                             return false;
                         }
                     return true;
                 }});

    // --- glTF: model zapisany w locie do .glb, wczytany ModelImporterem i zinstancjonowany ---
    v.push_back({"gltf_model", "Import .glb: plansza z tekstura szachownicy (baseColorTexture) i emisyjna zielona kula",
                 0.0f, Check::Custom, {},
                 [](Scene& s, Resources& r) {
                     MakeCamera(s, {0, 0, 4}, {0, 0, 0});
                     auto& sun = MakeLightObject(s, "Sun", {0, 0, 0}, {0.0f, 0.0f, -1.0f}).AddComponent<DirectionalLight>();
                     sun.intensity = 3.0f;

                     namespace tg = wz::testgltf;
                     tg::Document d;
                     tg::Primitive board; // plansza XY z UV (v = 0 u gory)
                     board.positions = {{-0.5f, -0.5f, 0}, {0.5f, -0.5f, 0}, {0.5f, 0.5f, 0}, {-0.5f, 0.5f, 0}};
                     board.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
                     board.uvs = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
                     board.indices = {0, 1, 2, 0, 2, 3};
                     board.material = 0;
                     tg::Primitive ball; // kula bez normalnych i UV - importer je uzupelnia
                     const MeshData sphere = MeshData::Sphere(0.6f, 32, 16);
                     for (const Vertex& vx : sphere.vertices) ball.positions.push_back(vx.position);
                     ball.indices = sphere.indices;
                     ball.material = 1;
                     d.meshes = {{"Board", {board}}, {"Ball", {ball}}};
                     tg::MaterialDesc checker;
                     checker.name = "Checker";
                     checker.metallic = 0.0f;
                     checker.roughness = 0.8f;
                     checker.baseColorImage = 0;
                     tg::MaterialDesc glow;
                     glow.name = "Glow";
                     glow.baseColor = {0.02f, 0.02f, 0.02f, 1.0f};
                     glow.metallic = 0.0f;
                     glow.emissive = {0.0f, 1.0f, 0.0f};
                     d.materials = {checker, glow};
                     // Szachownica 2x2 pola po 32 px: czerwony | bialy / bialy | czerwony
                     // (64x64, zeby filtrowanie liniowe nie rozmylo pol).
                     Image tex{64, 64, std::vector<std::uint8_t>(64 * 64 * 4, 255)};
                     for (int y = 0; y < 64; ++y)
                         for (int x = 0; x < 64; ++x)
                             if ((x < 32) == (y < 32)) {
                                 tex.pixels[(y * 64 + x) * 4 + 1] = 0;
                                 tex.pixels[(y * 64 + x) * 4 + 2] = 0;
                             }
                     d.pngImages.emplace_back();
                     tex.EncodePNG(d.pngImages.back());
                     d.nodes = {{"Root", -1, {1, 2}, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, false},
                                {"Board", 0, {}, {-0.9f, 0, 0}, {1, 0, 0, 0}, {1.6f, 1.6f, 1.6f}, true},
                                {"Ball", 1, {}, {1.0f, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, false}};
                     d.sceneRoots = {0};

                     const auto dir = std::filesystem::temp_directory_path() / "wz_render_gltf";
                     std::filesystem::create_directories(dir);
                     const auto file = dir / "board_and_ball.glb";
                     std::string err;
                     auto model = tg::Write(d, file, tg::Layout::Glb) ? ModelImporter::Load(file.string(), &err) : nullptr;
                     if (!model) {
                         std::fprintf(stderr, "  gltf_model: import nieudany: %s\n", err.c_str());
                         return; // scena bez modelu -> check nizej obleje
                     }
                     model->UploadToGpu(*r.device);
                     model->Instantiate(s);
                 },
                 [](const Image& img, const Scene& s, std::string& why) {
                     if (!s.Find("Board") || !s.Find("Ball")) { why = "model nie zostal zinstancjonowany"; return false; }
                     // Po UploadToGpu Mesh wspoldzieli geometrie CPU z MeshRendererem (MeshCollider).
                     const auto* boardMr = s.Find("Board")->GetComponent<MeshRenderer>();
                     if (!boardMr || !boardMr->mesh || !boardMr->cpuMesh || boardMr->mesh->GetCpuData() != boardMr->cpuMesh) {
                         why = "Mesh::GetCpuData() nie wskazuje na MeshRenderer::cpuMesh";
                         return false;
                     }
                     // Plansza: 1.6 x 1.6 wokol x = -0.9; srodki cwiartek +-0.4.
                     const glm::u8vec4 tl = PixelAtWorld(img, s, {-1.3f, 0.4f, 0.0f});
                     const glm::u8vec4 tr = PixelAtWorld(img, s, {-0.5f, 0.4f, 0.0f});
                     const glm::u8vec4 br = PixelAtWorld(img, s, {-0.5f, -0.4f, 0.0f});
                     const glm::u8vec4 ball = PixelAtWorld(img, s, {1.0f, 0.0f, 0.6f});
                     auto txt = [](glm::u8vec4 p) {
                         return "(" + std::to_string(p.r) + "," + std::to_string(p.g) + "," + std::to_string(p.b) + ")";
                     };
                     if (!(tl.r > 120 && tl.g + 60 < tl.r)) { why = "lewa gorna cwiartka nie jest czerwona " + txt(tl); return false; }
                     if (!(br.r > 120 && br.g + 60 < br.r)) { why = "prawa dolna cwiartka nie jest czerwona " + txt(br); return false; }
                     if (!(tr.g > 120 && std::abs(int(tr.r) - int(tr.g)) < 30)) { why = "prawa gorna cwiartka nie jest biala " + txt(tr); return false; }
                     if (!(ball.g > 150 && ball.r < 80 && ball.b < 80)) { why = "kula nie jest zielona " + txt(ball); return false; }
                     return true;
                 }});
    return v;
}

// --- test RHI: viewport ImGui zakodowany do sRGB DOKLADNIE RAZ (GetImGuiTextureId + BeginUiPass) ---
// Kolor celow to GL_SRGB8_ALPHA8, a GL_FRAMEBUFFER_SRGB jest wlaczone globalnie (scena: liniowo ->
// sRGB przy zapisie). Schemat B3 (wspolny z backendem edytora #12): GetImGuiTextureId zwraca widok
// UNORM RGBA8 (surowe bajty sRGB), a pass ImGui wylacza GL_FRAMEBUFFER_SRGB i przywraca stan po sobie.
// Wtedy bajty sceny trafiaja na ekran 1:1. Wojcimierz nie toleruje viewportu jasniejszego ani
// ciemniejszego niz scena - sprawdzamy to bajt po bajcie.
const char* kSrgbViewTestName = "srgb_imgui_view";

int LinearToSrgbByte(float x) {
    const float s = x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
    return static_cast<int>(s * 255.0f + 0.5f);
}
int SrgbByteToLinearByte(int b) {
    const float s = b / 255.0f;
    const float lin = s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
    return static_cast<int>(lin * 255.0f + 0.5f);
}

bool RunSrgbViewTest(rhi::Device& dev, const Resources& res, std::string& why) {
    constexpr int N = 16;
    static const char* kVs = R"(#version 450 core
void main() { // pelnoekranowy trojkat z gl_VertexID - atrybuty siatki ignorowane
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";
    static const char* kSolidFs = R"(#version 450 core
uniform vec4 u_Color;
out vec4 o_Color;
void main() { o_Color = u_Color; })";
    static const char* kBlitFs = R"(#version 450 core
layout(binding = 0) uniform sampler2D u_Tex;
out vec4 o_Color;
void main() { o_Color = texelFetch(u_Tex, ivec2(gl_FragCoord.xy), 0); })";

    const auto sceneRt = dev.CreateRenderTarget(N, N);
    const auto outRt = dev.CreateRenderTarget(N, N); // "ekran" edytora (tez sRGB - jedna sciezka)
    const auto solid = dev.CreateShader(kVs, kSolidFs);
    const auto blit = dev.CreateShader(kVs, kBlitFs);
    const auto vi = res.triangle->GetVertexInput();
    rhi::PipelineState ps;
    ps.depthTest = false;
    ps.depthWrite = false;
    ps.cullBackFaces = false;

    auto draw = [&](rhi::RenderTargetHandle rt, rhi::ShaderHandle sh, auto&& setup) {
        dev.BeginPass(rt, N, N, glm::vec4(0, 0, 0, 1));
        dev.SetPipelineState(ps);
        dev.BindShader(sh);
        setup();
        dev.DrawIndexed(vi, 3);
        dev.EndPass();
    };
    auto center = [&](rhi::RenderTargetHandle rt) {
        dev.Finish();
        const auto px = dev.ReadPixels(rt, N, N); // surowe bajty (bez dekodowania sRGB)
        const std::size_t i = (static_cast<std::size_t>(N / 2) * N + N / 2) * 4;
        return glm::ivec4(px[i], px[i + 1], px[i + 2], px[i + 3]);
    };
    auto blitFrom = [&](std::uint32_t texName) {
        draw(outRt, blit, [&] { dev.BindTexture(0, rhi::TextureHandle{texName}); });
        return center(outRt);
    };
    auto idOf = [](void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); };
    auto fmt = [](glm::ivec4 c) {
        return "(" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + ")";
    };

    bool ok = true;
    auto fail = [&](const std::string& m) { if (ok) why = m; ok = false; };
    const glm::vec4 linear(0.5f, 0.25f, 0.75f, 1.0f);

    // 1. Scena: shader oddaje wartosci liniowe, zapis koduje do sRGB raz (sprzetowo).
    if (!dev.IsFramebufferSrgbEnabled()) fail("GL_FRAMEBUFFER_SRGB powinno byc wlaczone dla sceny");
    draw(sceneRt, solid, [&] { dev.SetUniform(solid, "u_Color", linear); });
    const glm::ivec4 raw = center(sceneRt);
    for (int c = 0; c < 3; ++c)
        if (std::abs(raw[c] - LinearToSrgbByte(linear[c])) > 1)
            fail("scena " + fmt(raw) + " nie jest zakodowana do sRGB dokladnie raz");

    // 2. Identyfikator ImGui: widok UNORM (rozny od tekstury sRGB), staly miedzy wywolaniami.
    const std::uint32_t view = idOf(dev.GetImGuiTextureId(sceneRt));
    const std::uint32_t srgbColor = dev.GetColorTexture(sceneRt).id;
    if (view == 0 || view == srgbColor) fail("GetImGuiTextureId nie zwraca osobnego widoku UNORM");
    if (idOf(dev.GetImGuiTextureId(sceneRt)) != view) fail("widok ImGui nie jest staly");

    // 3. Pass ImGui: kodowanie wylaczone -> bajty sceny 1:1 (zakodowane dokladnie raz).
    glm::ivec4 viaUi, decoded;
    {
        rhi::ScopedUiPass ui(dev);
        if (dev.IsFramebufferSrgbEnabled()) fail("BeginUiPass nie wylaczyl GL_FRAMEBUFFER_SRGB");
        viaUi = blitFrom(view);
        // Kontrola: sama tekstura sRGB dekoduje do liniowych - viewport bylby za ciemny.
        decoded = blitFrom(srgbColor);
    }
    if (!dev.IsFramebufferSrgbEnabled()) fail("EndUiPass nie przywrocil GL_FRAMEBUFFER_SRGB");
    for (int c = 0; c < 3; ++c) {
        if (std::abs(viaUi[c] - raw[c]) > 1) fail("viewport ImGui " + fmt(viaUi) + " != bajty sceny " + fmt(raw));
        if (std::abs(decoded[c] - SrgbByteToLinearByte(raw[c])) > 2)
            fail("tekstura sRGB " + fmt(decoded) + " nie dekoduje sie do liniowych (surowe " + fmt(raw) + ")");
    }

    // 4. Kontrola odwrotna: widok UNORM BEZ passu UI -> drugie kodowanie (za jasno). Dlatego pass UI.
    const glm::ivec4 twice = blitFrom(view);
    if (ok && twice.r <= viaUi.r) fail("bez BeginUiPass widok powinien byc zakodowany drugi raz (jasniej)");

    // 5. Zagniezdzanie: przywracany jest stan sprzed DANEGO BeginUiPass.
    dev.BeginUiPass();
    dev.BeginUiPass();
    dev.EndUiPass();
    if (dev.IsFramebufferSrgbEnabled()) fail("zagniezdzony EndUiPass wlaczyl kodowanie za wczesnie");
    dev.EndUiPass();
    if (!dev.IsFramebufferSrgbEnabled()) fail("zewnetrzny EndUiPass nie przywrocil kodowania");

    if (ok)
        why = "scena " + fmt(raw) + " ImGui " + fmt(viaUi) + " (raz), bez passu UI " + fmt(twice) +
              " (dwa razy), tekstura sRGB " + fmt(decoded);

    dev.DestroyShader(solid);
    dev.DestroyShader(blit);
    dev.DestroyRenderTarget(sceneRt);
    dev.DestroyRenderTarget(outRt);
    dev.Finish();
    return ok;
}

struct Options {
    std::string outDir = WZ_RENDER_OUT_DIR;
    std::string only;
    std::string referenceDir;
    int size = 256;
    int threshold = 2;
    double maxDiffRatio = 0.001;
    bool list = false;
};

// ============================================================================
// HOOK DLA KOMPARATORA TESTERA (golden images)
// ----------------------------------------------------------------------------
// compareWithGolden() jest JEDYNYM miejscem, przez ktore przechodzi ocena
// kazdej sceny. Obecnie robi tylko podstawowy sanity-check (oczekiwany kolor
// tla / obraz niejednolity) oraz - opcjonalnie, z --reference DIR - proste
// porownanie z wzorcem. Tu wpina sie komparator zespolu testow (np. zapis
// <scena>_diff.png do render_out/, tolerancje per scena, raport JSON).
// ============================================================================
struct GoldenResult {
    bool ok = true;
    std::string message;
};

GoldenResult compareWithGolden(const TestScene& ts, const Image& img, const Options& opt);

bool Parse(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : nullptr; };
        const char* v = nullptr;
        if (a == "--list") o.list = true;
        else if (a == "--out" && (v = need())) o.outDir = v;
        else if (a == "--scene" && (v = need())) o.only = v;
        else if (a == "--reference" && (v = need())) o.referenceDir = v;
        else if (a == "--size" && (v = need())) o.size = std::atoi(v);
        else if (a == "--threshold" && (v = need())) o.threshold = std::atoi(v);
        else if (a == "--max-diff-ratio" && (v = need())) o.maxDiffRatio = std::atof(v);
        else { std::fprintf(stderr, "Nieznana/niepelna opcja: %s\n", a.c_str()); return false; }
    }
    return o.size > 0;
}


GoldenResult compareWithGolden(const TestScene& ts, const Image& img, const Options& opt) {
    GoldenResult r;
    // 1) Sanity-check zalezny od sceny.
    if (ts.check == Check::UniformColor) {
        for (std::size_t i = 0; i < img.pixels.size(); i += 4)
            for (int c = 0; c < 4; ++c)
                if (std::abs(int(img.pixels[i + c]) - int(ts.expectedColor[c])) > 2) // +-2/255
                    return {false, "piksel rozny od oczekiwanego koloru tla"};
    } else if (ts.check == Check::NotUniform && img.CountUniqueColors() < 2) {
        return {false, "obraz jednolity - nic sie nie wyrenderowalo"};
    }
    // 2) Opcjonalne porownanie z wzorcem (--reference DIR).
    if (!opt.referenceDir.empty()) {
        const std::string refPath = (std::filesystem::path(opt.referenceDir) / (ts.name + ".png")).string();
        Image ref;
        if (!Image::LoadPNG(refPath, ref)) {
            std::printf("  (brak wzorca %s - pomijam porownanie)\n", refPath.c_str());
            return r;
        }
        const ImageDiff d = CompareImages(img, ref, opt.threshold);
        if (d.sizeMismatch) return {false, "inny rozmiar niz wzorzec"};
        if (d.differingRatio > opt.maxDiffRatio)
            return {false, "rozni sie od wzorca: " + std::to_string(d.differingPixels) + " px (" +
                               std::to_string(d.differingRatio * 100.0) + "%)"};
    }
    // TODO(tester): komparator golden images (diff PNG, raport) - tutaj.
    return r;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!Parse(argc, argv, opt)) return 1;
    auto scenes = MakeScenes();
    if (opt.list) {
        for (auto& s : scenes) std::printf("%-14s t=%.3fs  %s\n", s.name.c_str(), s.simulateSeconds, s.description.c_str());
        std::printf("%-14s test RHI: viewport ImGui zakodowany do sRGB dokladnie raz\n", kSrgbViewTestName);
        return 0;
    }

    try {
        WindowDesc wd;
        wd.title = "render_tests";
        wd.width = opt.size;
        wd.height = opt.size;
        wd.visible = false;
        wd.vsync = false;
        wd.resizable = false;
        // Brak X/xvfb albo sterownika z OpenGL 4.5 -> pomijamy (kod 77), zamiast oblewac CI bez GPU.
        std::unique_ptr<Window> windowPtr;
        std::unique_ptr<rhi::Device> device;
        try {
            windowPtr = std::make_unique<Window>(wd);
            device = rhi::CreateOpenGLDevice(&Window::GetProcAddress);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[render_tests] POMINIETO - brak kontekstu OpenGL 4.5: %s\n", e.what());
            return 77;
        }
        const auto info = device->GetInfo();
        std::printf("[render_tests] %s | %s | %s\n", info.renderer.c_str(), info.version.c_str(), opt.outDir.c_str());

        Resources res{device.get(), Shader::CreateDefaultLit(*device), Mesh::Create(*device, MeshData::Cube()),
                      Mesh::Create(*device, MeshData::Triangle()), Mesh::Create(*device, MeshData::Quad()),
                      Mesh::Create(*device, MeshData::Sphere(0.8f, 48, 24)), nullptr};
        {
            MeshData split = MeshData::Cube();
            split.submeshes = {{0, 18}, {18, 18}};
            res.splitCube = Mesh::Create(*device, split);
        }
        Renderer renderer(*device);
        auto target = device->CreateRenderTarget(opt.size, opt.size);
        std::filesystem::create_directories(opt.outDir);

        int failures = 0, ran = 0;
        for (const TestScene& ts : scenes) {
            if (!opt.only.empty() && opt.only != ts.name) continue;
            ++ran;
            Time::Reset();
            Scene scene(ts.name);
            ts.build(scene, res);

            // Symulacja: staly krok, ManualClock - zero zaleznosci od prawdziwego czasu.
            ManualClock clock;
            GameLoop loop(GameLoop::Settings{kFixedDt, 0.25f});
            loop.SetClock(&clock);
            loop.SetCallbacks({[&](float dt) { scene.FixedUpdate(dt); }, [&](float dt) { scene.Update(dt); },
                               [&](float dt) { scene.LateUpdate(dt); }, nullptr});
            loop.SyncClock();
            const int frames = static_cast<int>(ts.simulateSeconds / kFixedDt + 0.5f);
            for (int f = 0; f < frames; ++f) { clock.Tick(kFixedDt); loop.Step(); }
            scene.RunPendingStarts();

            renderer.Render(scene, target, opt.size, opt.size);
            device->Finish();
            Image img{opt.size, opt.size, device->ReadPixels(target, opt.size, opt.size)};
            const std::string path = (std::filesystem::path(opt.outDir) / (ts.name + ".png")).string();
            bool ok = img.SavePNG(path);
            std::string why = ok ? "" : "zapis PNG";

            const std::size_t colors = img.CountUniqueColors();
            if (ok && ts.check == Check::Custom) {
                // Sceny z wlasna kontrola (PBR) - potrzebuja sceny, wiec oceniane tutaj.
                ok = ts.custom && ts.custom(img, scene, why);
            }
            if (ok) {
                const GoldenResult g = compareWithGolden(ts, img, opt);
                ok = g.ok;
                why = g.message;
            }

            std::printf("[%s] %-14s %dx%d t=%.3fs klatek=%d krokow_fixed=%llu kolorow=%zu -> %s %s\n", ok ? " OK " : "FAIL",
                        ts.name.c_str(), opt.size, opt.size, ts.simulateSeconds, frames,
                        static_cast<unsigned long long>(Time::fixedStepCount), colors, path.c_str(), why.c_str());
            if (!ok) ++failures;
        }
        device->DestroyRenderTarget(target);

        if (opt.only.empty() || opt.only == kSrgbViewTestName) {
            ++ran;
            std::string why;
            const bool ok = RunSrgbViewTest(*device, res, why);
            std::printf("[%s] %-14s %s\n", ok ? " OK " : "FAIL", kSrgbViewTestName, why.c_str());
            if (!ok) ++failures;
        }
        if (ran == 0) { std::fprintf(stderr, "Brak sceny o nazwie '%s' (uzyj --list)\n", opt.only.c_str()); return 1; }
        std::printf("[render_tests] scen: %d, bledow: %d\n", ran, failures);
        return failures == 0 ? 0 : 4;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[render_tests] blad: %s\n", e.what());
        return 3;
    }
}
