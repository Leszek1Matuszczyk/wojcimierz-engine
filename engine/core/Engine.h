#pragma once
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "platform/Window.h"
#include "render/RHI.h"

#include <memory>
#include <string>

namespace wz {

class Scene;
class Renderer;
class IInputSource;

struct EngineConfig {
    WindowDesc window;
    bool headless = false;          // niewidoczne okno + render do offscreen, staly krok czasu
    int maxFrames = 0;              // 0 = bez limitu (tryb okienkowy)
    std::string screenshotPath;     // zrzut ostatniej klatki do PNG (pusty = brak)
    float fixedDeltaTime = 1.0f / 60.0f;
    float headlessFrameTime = 1.0f / 60.0f; // czas klatki w trybie headless (deterministyczny)

    // Parsuje: --headless --frames N --screenshot plik.png --width W --height H
    // Zwraca false przy blednych argumentach (komunikat w 'error').
    static bool ParseArgs(int argc, char** argv, EngineConfig& inOut, std::string& error);
};

// Glowny obiekt silnika: okno, urzadzenie RHI, scena, renderer i petla gry.
class Engine {
public:
    explicit Engine(EngineConfig config);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Scene& GetScene() { return *scene_; }
    rhi::Device& GetDevice() { return *device_; }
    Renderer& GetRenderer() { return *renderer_; }
    Window& GetWindow() { return *window_; }
    const EngineConfig& GetConfig() const { return config_; }
    GameLoop& GetLoop() { return loop_; }

    // Wstrzykniecie zegara (np. ManualClock w testach). Bez wywolania:
    // tryb okienkowy -> SystemClock, headless -> wewnetrzny ManualClock (+headlessFrameTime na klatke).
    void SetClock(IClock* clock);
    // Wstrzykniecie zrodla wejscia (np. FakeInput). Domyslnie GLFW.
    void SetInputSource(IInputSource* source);

    // Uruchamia petle gry. Zwraca kod wyjscia procesu (0 = OK).
    int Run();
    // Jedna klatka: zdarzenia okna, Input::Update, krok petli (FixedUpdate*/Update/LateUpdate) i render.
    void RunFrame();
    void RequestQuit() { quit_ = true; }

    bool SaveScreenshot(const std::string& path);

private:
    EngineConfig config_;
    std::unique_ptr<Window> window_;
    std::unique_ptr<rhi::Device> device_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Scene> scene_;
    std::unique_ptr<IInputSource> glfwInput_;
    SystemClock systemClock_;
    ManualClock headlessClock_;
    IClock* clock_ = nullptr;
    bool usingHeadlessClock_ = false;
    GameLoop loop_;
    rhi::RenderTargetHandle offscreen_;
    bool quit_ = false;
};

} // namespace wz
