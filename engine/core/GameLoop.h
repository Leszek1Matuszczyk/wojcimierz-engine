// =============================================================================
// Wojcimierz Engine - engine/core/GameLoop.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include <functional>

#include "core/Clock.h"

namespace wz {

// Petla gry z krokiem stalym (FixedUpdate, fizyka) i zmiennym (Update).
// Klasyczny akumulator ("Fix Your Timestep!"): czas klatki dodawany jest do
// akumulatora, a FixedUpdate wykonuje sie tyle razy, ile pelnych krokow
// fixedDeltaTime sie w nim miesci. Petla nie zna zegara systemowego -
// dostaje czas klatki z zewnatrz, wiec jest w pelni deterministyczna i testowalna.
class GameLoop {
public:
    struct Callbacks {
        std::function<void(float fixedDt)> fixedUpdate;
        std::function<void(float dt)> update;
        std::function<void(float dt)> lateUpdate;
        std::function<void(float alpha)> render; // alpha = interpolacja miedzy krokami stalymi
    };

    struct Settings {
        // Krok staly trzymamy w double od konca do konca (Settings -> akumulator -> Time),
        // zeby czas symulacji nie dryfowal; do FixedUpdate(float) komponentow rzutujemy dopiero przy wywolaniu.
        double fixedDeltaTime = 1.0 / 60.0;
        double maximumDeltaTime = 0.25; // jak Time.maximumDeltaTime w Unity (ochrona przed "spirala smierci")
    };

    GameLoop() = default;
    explicit GameLoop(Settings s) : settings_(s) {}

    void SetCallbacks(Callbacks cb) { callbacks_ = std::move(cb); }
    const Settings& GetSettings() const { return settings_; }

    // Wykonuje jedna klatke z podanym czasem klatki. Zwraca liczbe krokow FixedUpdate.
    // NaN/ujemny czas -> 0, +inf/dlugi -> maximumDeltaTime; ujemny Time::timeScale -> 0.
    int Tick(double realFrameSeconds);

    // Wstrzykniecie zegara (nie przejmuje wlasnosci). nullptr = brak zegara.
    void SetClock(const IClock* clock);
    const IClock* GetClock() const { return clock_; }
    // Wykonuje jedna klatke, mierzac czas klatki wstrzyknietym zegarem
    // (roznica Now() od poprzedniego wywolania; pierwsza klatka ma dt = 0).
    int Step();
    // Zapamietuje biezacy odczyt zegara jako punkt odniesienia bez wykonywania klatki.
    void SyncClock();

    double Accumulator() const { return accumulator_; }
    float Alpha() const;

private:
    Settings settings_{};
    Callbacks callbacks_{};
    double accumulator_ = 0.0;
    double fixedTimeCompensation_ = 0.0; // kompensacja Kahana dla Time::fixedTime
    double lastFixedTime_ = 0.0;         // ostatnia zapisana wartosc Time::fixedTime (wykrywa Time::Reset)
    const IClock* clock_ = nullptr;
    double lastClockTime_ = 0.0;
    bool hasLastClockTime_ = false;
};

} // namespace wz
