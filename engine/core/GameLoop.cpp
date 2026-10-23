#include "core/GameLoop.h"
#include "core/Time.h"

#include <algorithm>

namespace wz {

int GameLoop::Tick(double realFrameSeconds) {
    if (realFrameSeconds < 0.0) realFrameSeconds = 0.0;
    const double clamped = std::min(realFrameSeconds, static_cast<double>(settings_.maximumDeltaTime));
    const double scaled = clamped * Time::timeScale;

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
    if (settings_.fixedDeltaTime <= 0.0f) return 0.0f;
    return static_cast<float>(accumulator_ / settings_.fixedDeltaTime);
}

} // namespace wz
