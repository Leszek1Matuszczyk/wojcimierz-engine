// =============================================================================
// Wojcimierz Engine - engine/core/Engine.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/Engine.h"
#include "core/Time.h"
#include "platform/GlfwInputSource.h"
#include "platform/Input.h"
#include "render/Image.h"
#include "render/RenderComponents.h"
#include "render/Renderer.h"
#include "scene/Scene.h"
#include "scene/SceneComponents.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wz {

bool EngineConfig::ParseArgs(int argc, char** argv, EngineConfig& c, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&](const char* opt) -> const char* {
            if (i + 1 >= argc) { error = std::string("Brak wartosci dla ") + opt; return nullptr; }
            return argv[++i];
        };
        if (!std::strcmp(a, "--headless")) c.headless = true;
        else if (!std::strcmp(a, "--force-srgb-blit")) c.forceSrgbBlit = true;
        else if (!std::strcmp(a, "--frames")) { auto v = next(a); if (!v) return false; c.maxFrames = std::atoi(v); }
        else if (!std::strcmp(a, "--screenshot")) { auto v = next(a); if (!v) return false; c.screenshotPath = v; }
        else if (!std::strcmp(a, "--width")) { auto v = next(a); if (!v) return false; c.window.width = std::atoi(v); }
        else if (!std::strcmp(a, "--height")) { auto v = next(a); if (!v) return false; c.window.height = std::atoi(v); }
        else { error = std::string("Nieznana opcja: ") + a; return false; }
    }
    if (c.headless && c.maxFrames <= 0) c.maxFrames = 1;
    if (c.window.width <= 0 || c.window.height <= 0) { error = "Niepoprawny rozmiar"; return false; }
    return true;
}

Engine::Engine(EngineConfig config) : config_(std::move(config)), loop_(GameLoop::Settings{config_.fixedDeltaTime, config_.maximumDeltaTime}) {
    // Jawna rejestracja komponentów, zanim cokolwiek wczyta scenę (decyzja zespołu: bez whole-archive).
    // Engine nie ma osobnego Init, więc to pierwsza rzecz w konstruktorze, na wątku głównym.
    RegisterSceneComponents();
    RegisterRenderComponents(); // MeshRenderer i swiatla - po scenie, bo dziedzicza po Component
    Time::Reset();
    Input::Reset();
    WindowDesc wd = config_.window;
    if (config_.headless) { wd.visible = false; wd.vsync = false; }
    window_ = std::make_unique<Window>(wd);
    device_ = rhi::CreateOpenGLDevice(&Window::GetProcAddress);
    renderer_ = std::make_unique<Renderer>(*device_);
    scene_ = std::make_unique<Scene>("Main");
    glfwInput_ = std::make_unique<GlfwInputSource>(window_->GetNativeHandle());
    Input::SetSource(glfwInput_.get());

    if (config_.headless) {
        offscreen_ = device_->CreateRenderTarget(config_.window.width, config_.window.height);
        clock_ = &headlessClock_;
        usingHeadlessClock_ = true;
        std::printf("[Wojcimierz] sRGB: tryb headless - render do celu GL_SRGB8_ALPHA8\n");
    } else {
        clock_ = &systemClock_;
        if (device_->IsDefaultFramebufferSRGB() && !config_.forceSrgbBlit) {
            std::printf("[Wojcimierz] sRGB: back buffer okna jest sRGB - render bezposrednio do okna\n");
        } else {
            // Okno bez sRGB: renderujemy do wewnetrznego celu sRGB i kopiujemy surowe bajty na ekran,
            // zeby okno i zrzuty ekranu zawsze wygladaly tak samo.
            blitToWindow_ = true;
            EnsureWindowTarget();
            std::printf("[Wojcimierz] sRGB: back buffer okna NIE jest sRGB - render do GL_SRGB8_ALPHA8 + blit do okna\n");
        }
    }
    loop_.SetClock(clock_);
    loop_.SetCallbacks({
        [this](float dt) { fixedSystems_.Run(dt, [this](float d) { scene_->FixedUpdate(d); }); },
        [this](float dt) { scene_->Update(dt); },
        [this](float dt) { lateSystems_.Run(dt, [this](float d) { scene_->LateUpdate(d); }); },
        [this](float) {
            int w = config_.window.width, h = config_.window.height;
            if (!config_.headless) window_->GetFramebufferSize(w, h);
            if (blitToWindow_) EnsureWindowTarget();
            renderer_->Render(*scene_, offscreen_, w, h);
            if (blitToWindow_ && w > 0 && h > 0) device_->BlitToDefault(offscreen_, w, h, w, h);
        },
    });

    const auto info = device_->GetInfo();
    std::printf("[Wojcimierz] %s | %s | %s | %s\n", info.backend.c_str(), info.vendor.c_str(),
                info.renderer.c_str(), info.version.c_str());
}

Engine::~Engine() {
    if (Input::GetSource() == glfwInput_.get()) Input::SetSource(nullptr);
    scene_.reset(); // komponenty moga trzymac zasoby GPU - zwalniamy przed urzadzeniem
    if (offscreen_) device_->DestroyRenderTarget(offscreen_);
    renderer_.reset();
    device_.reset();
    window_.reset();
}

void Engine::SetClock(IClock* clock) {
    clock_ = clock ? clock : (config_.headless ? static_cast<IClock*>(&headlessClock_) : &systemClock_);
    usingHeadlessClock_ = (clock_ == &headlessClock_);
    loop_.SetClock(clock_);
}

void Engine::SetInputSource(IInputSource* source) { Input::SetSource(source ? source : glfwInput_.get()); }

void Engine::RunFrame() {
    window_->PollEvents();
    Input::Update();
    loop_.Step();
    if (usingHeadlessClock_) headlessClock_.Tick(config_.headlessFrameTime);
}

void Engine::EnsureWindowTarget() {
    int w = 0, h = 0;
    window_->GetFramebufferSize(w, h);
    if (w <= 0 || h <= 0) return; // zminimalizowane okno
    if (offscreen_ && w == offscreenW_ && h == offscreenH_) return;
    if (offscreen_) device_->DestroyRenderTarget(offscreen_);
    offscreen_ = device_->CreateRenderTarget(w, h);
    offscreenW_ = w;
    offscreenH_ = h;
}

bool Engine::SaveScreenshot(const std::string& path) {
    int w = config_.window.width, h = config_.window.height;
    if (!config_.headless) window_->GetFramebufferSize(w, h);
    device_->Finish();
    Image img;
    img.width = w;
    img.height = h;
    img.pixels = device_->ReadPixels(offscreen_, w, h);
    if (blitToWindow_) {
        // Kontrola: okno (po blicie) musi miec te same bajty co zrzut z celu sRGB.
        const auto window = device_->ReadPixels({}, w, h);
        std::size_t bad = 0;
        for (std::size_t i = 0; i < window.size() && i < img.pixels.size(); ++i)
            if (std::abs(int(window[i]) - int(img.pixels[i])) > 2 && (i % 4) != 3) ++bad;
        std::printf("[Wojcimierz] sRGB: okno vs zrzut - rozne kanaly: %zu\n", bad);
        if (bad != 0) return false;
    }
    const bool ok = img.SavePNG(path);
    std::printf("[Wojcimierz] zrzut %s (%dx%d, kolorow: %zu): %s\n", path.c_str(), w, h, img.CountUniqueColors(),
                ok ? "OK" : "BLAD");
    return ok;
}

int Engine::Run() {
    // Punkt odniesienia zegara. W trybie headless od razu przesuwamy zegar reczny,
    // zeby kazda klatka (takze pierwsza) miala dt = headlessFrameTime.
    loop_.SyncClock();
    if (usingHeadlessClock_) headlessClock_.Tick(config_.headlessFrameTime);
    int frames = 0;
    while (!quit_ && !window_->ShouldClose()) {
        RunFrame();
        ++frames;
        const bool last = config_.maxFrames > 0 && frames >= config_.maxFrames;
        if (last && !config_.screenshotPath.empty()) {
            if (!SaveScreenshot(config_.screenshotPath)) return 2;
        }
        if (!config_.headless) window_->SwapBuffers();
        if (last) break;
    }
    return 0;
}

} // namespace wz
