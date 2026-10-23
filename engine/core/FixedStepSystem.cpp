// =============================================================================
// Wojcimierz Engine - engine/core/FixedStepSystem.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/FixedStepSystem.h"

namespace wz {

void FixedStepPipeline::Run(float dt, const std::function<void(float)>& componentsFixedUpdate) {
    // Jeden zakres przebiegu: systemy dodane w trakcie czekaja do nastepnego kroku,
    // a usuniete w Pre nie dostaja Post.
    SystemList<IFixedStepSystem>::RunScope run(systems_);
    run.ForEach([dt](IFixedStepSystem& s) { s.PreFixedUpdate(dt); });
    if (componentsFixedUpdate) componentsFixedUpdate(dt);
    run.ForEach([dt](IFixedStepSystem& s) { s.PostFixedUpdate(dt); });
}

void LateUpdatePipeline::Run(float dt, const std::function<void(float)>& componentsLateUpdate) {
    if (componentsLateUpdate) componentsLateUpdate(dt);
    systems_.ForEach([dt](ILateUpdateSystem& s) { s.OnLateUpdate(dt); });
}

} // namespace wz
