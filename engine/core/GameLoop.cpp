// =============================================================================
// Wojcimierz Engine - engine/core/GameLoop.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/GameLoop.h"
#include "core/Time.h"

#include <algorithm>
#include <cmath>

namespace wz {

int GameLoop::Tick(double realFrameSeconds) {
    // Ochrona przed zlym czasem klatki (issue #1):
    //  * NaN albo ujemny -> klatka z dt = 0 (nie zatruwa Time::time ani akumulatora),
    //  * +inf / bardzo dlugi -> przyciecie do maximumDeltaTime (ochrona przed "spirala smierci").
    if (std::isnan(realFrameSeconds) || realFrameSeconds < 0.0) realFrameSeconds = 0.0;
    double maxDt = static_cast<double>(settings_.maximumDeltaTime);
    if (!std::isfinite(maxDt) || maxDt < 0.0) maxDt = 0.25;
    const double clamped = std::min(realFrameSeconds, maxDt);

    // Ujemny lub niepoprawny timeScale (issue #2) - czas gry nigdy sie nie cofa.
    if (!std::isfinite(Time::timeScale) || Time::timeScale < 0.0f) Time::timeScale = 0.0f;
    const double scaled = clamped * static_cast<double>(Time::timeScale);

    Time::fixedDeltaTime = settings_.fixedDeltaTime;
    Time::unscaledDeltaTime = static_cast<float>(clamped);
    Time::unscaledTime += clamped;

    accumulator_ += scaled;
    int steps = 0;
    const double fixedDt = settings_.fixedDeltaTime;
    // Mala tolerancja chroni przed bledami zaokraglen (np. 3 * (1/60) w double).
    constexpr double kEpsilon = 1e-9;
    while (fixedDt > 0.0 && accumulator_ + kEpsilon >= fixedDt) {
        Time::inFixedTimeStep = true;
        Time::deltaTime = settings_.fixedDeltaTime;
        if (callbacks_.fixedUpdate) callbacks_.fixedUpdate(settings_.fixedDeltaTime);
        Time::fixedTime += fixedDt;
        ++Time::fixedStepCount;
        accumulator_ -= fixedDt;
        if (accumulator_ < 0.0) accumulator_ = 0.0;
        ++steps;
    }
    Time::inFixedTimeStep = false;

    Time::deltaTime = static_cast<float>(scaled);
    Time::time += scaled;

    if (callbacks_.update) callbacks_.update(Time::deltaTime);
    if (callbacks_.lateUpdate) callbacks_.lateUpdate(Time::deltaTime);
    if (callbacks_.render) callbacks_.render(Alpha());
    ++Time::frameCount;
    return steps;
}

void GameLoop::SetClock(const IClock* clock) {
    clock_ = clock;
    hasLastClockTime_ = false;
}

int GameLoop::Step() {
    if (!clock_) return Tick(0.0);
    const double now = clock_->Now();
    const double dt = hasLastClockTime_ ? now - lastClockTime_ : 0.0;
    lastClockTime_ = now;
    hasLastClockTime_ = true;
    return Tick(dt);
}

void GameLoop::SyncClock() {
    if (!clock_) return;
    lastClockTime_ = clock_->Now();
    hasLastClockTime_ = true;
}

float GameLoop::Alpha() const {
    if (!(settings_.fixedDeltaTime > 0.0f)) return 0.0f;
    const float a = static_cast<float>(accumulator_ / settings_.fixedDeltaTime);
    if (!std::isfinite(a) || a < 0.0f) return 0.0f;
    return a < 1.0f ? a : std::nextafter(1.0f, 0.0f); // zakres [0, 1)
}

} // namespace wz
