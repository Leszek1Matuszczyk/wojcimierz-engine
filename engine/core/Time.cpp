// =============================================================================
// Wojcimierz Engine - engine/core/Time.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/Time.h"

namespace wz {

void Time::Reset() {
    time = 0.0;
    unscaledTime = 0.0;
    deltaTime = 0.0f;
    unscaledDeltaTime = 0.0f;
    fixedDeltaTime = 1.0 / 60.0;
    maximumDeltaTime = 0.25;
    timeScale = 1.0f;
    fixedTime = 0.0;
    frameCount = 0;
    fixedStepCount = 0;
    inFixedTimeStep = false;
}

} // namespace wz
