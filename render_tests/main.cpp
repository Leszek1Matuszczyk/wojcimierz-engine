// =============================================================================
// Wojcimierz Engine - render_tests/main.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
// render_tests: deterministyczne sceny testowe renderowane offscreen do PNG.
//
//   render_tests [--out DIR] [--scene NAZWA] [--list] [--size N]
//                [--reference DIR] [--threshold T] [--max-diff-ratio R]
//
// Kazda scena: staly rozmiar (domyslnie 256x256), staly krok czasu (1/60 s),
// staly czas symulacji, ziarno losowania 1337 -> te same piksele przy kazdym uruchomieniu
// na tym samym sterowniku (CI: Mesa llvmpipe).
// Wynik: <DIR>/<scena>.png (domyslnie build/render_out/).
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "platform/Window.h"
#include "render/Camera.h"
#include "render/Image.h"
#include "render/Mesh.h"
#include "render/MeshRenderer.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

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
    std::shared_ptr<Mesh> cube, triangle, quad;
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

enum class Check { UniformColor, NotUniform };

struct TestScene {
    std::string name;
    std::string description;
    float simulateSeconds; // czas symulacji przed zrzutem (staly krok)
    Check check;
    glm::u8vec4 expectedColor{0};
    std::function<void(Scene&, Resources&)> build;
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
    return v;
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
    } else if (img.CountUniqueColors() < 2) {
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
        Window window(wd);
        auto device = rhi::CreateOpenGLDevice(&Window::GetProcAddress);
        const auto info = device->GetInfo();
        std::printf("[render_tests] %s | %s | %s\n", info.renderer.c_str(), info.version.c_str(), opt.outDir.c_str());

        Resources res{device.get(), Shader::CreateDefaultLit(*device), Mesh::Create(*device, MeshData::Cube()),
                      Mesh::Create(*device, MeshData::Triangle()), Mesh::Create(*device, MeshData::Quad())};
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
        if (ran == 0) { std::fprintf(stderr, "Brak sceny o nazwie '%s' (uzyj --list)\n", opt.only.c_str()); return 1; }
        std::printf("[render_tests] scen: %d, bledow: %d\n", ran, failures);
        return failures == 0 ? 0 : 4;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[render_tests] blad: %s\n", e.what());
        return 3;
    }
}
