// =============================================================================
// Wojcimierz Engine - engine/core/Engine.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include "core/Clock.h"
#include "core/FixedStepSystem.h"
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

    // Systemy kroku stalego (np. PhysicsWorld). W kazdym kroku: PreFixedUpdate systemow ->
    // FixedUpdate komponentow -> PostFixedUpdate systemow (Unity: skrypty, potem krok fizyki).
    // Engine nie przejmuje wlasnosci; system musi zyc dluzej niz rejestracja.
    void AddFixedStepSystem(IFixedStepSystem* system, int order = 0) { fixedSystems_.Add(system, order); }
    bool RemoveFixedStepSystem(IFixedStepSystem* system) { return fixedSystems_.Remove(system); }
    FixedStepPipeline& GetFixedStepPipeline() { return fixedSystems_; }
    // Systemy wolane raz na klatke po LateUpdate komponentow, przed renderem (np. audio).
    void AddLateUpdateSystem(ILateUpdateSystem* system, int order = 0) { lateSystems_.Add(system, order); }
    bool RemoveLateUpdateSystem(ILateUpdateSystem* system) { return lateSystems_.Remove(system); }
    LateUpdatePipeline& GetLateUpdatePipeline() { return lateSystems_; }

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
    FixedStepPipeline fixedSystems_;
    LateUpdatePipeline lateSystems_;
    rhi::RenderTargetHandle offscreen_;
    bool quit_ = false;
};

} // namespace wz
