// =============================================================================
// Wojcimierz Engine - engine/core/FixedStepSystem.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/FixedStepSystem.h"

#include <algorithm>

namespace wz {

void FixedStepPipeline::Add(IFixedStepSystem* system, int order) {
    if (!system || Contains(system)) return;
    Entry e{system, order, nextSeq_++};
    if (running_ > 0) {
        // W trakcie Run(): dopisz na koniec (aktywny od nastepnego kroku), posortuj po zakonczeniu.
        entries_.push_back(e);
        needsCompact_ = true;
        return;
    }
    auto it = std::upper_bound(entries_.begin(), entries_.end(), e, [](const Entry& a, const Entry& b) {
        return a.order != b.order ? a.order < b.order : a.seq < b.seq;
    });
    entries_.insert(it, e);
}

bool FixedStepPipeline::Remove(IFixedStepSystem* system) {
    for (auto& e : entries_) {
        if (e.system == system) {
            e.system = nullptr;
            needsCompact_ = true;
            if (running_ == 0) Compact();
            return true;
        }
    }
    return false;
}

bool FixedStepPipeline::Contains(const IFixedStepSystem* system) const {
    if (!system) return false;
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.system == system; });
}

std::size_t FixedStepPipeline::Size() const {
    return static_cast<std::size_t>(
        std::count_if(entries_.begin(), entries_.end(), [](const Entry& e) { return e.system != nullptr; }));
}

void FixedStepPipeline::Compact() {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [](const Entry& e) { return !e.system; }),
                   entries_.end());
    std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        return a.order != b.order ? a.order < b.order : a.seq < b.seq;
    });
    needsCompact_ = false;
}

void FixedStepPipeline::Run(float dt, const std::function<void(float)>& componentsFixedUpdate) {
    ++running_;
    const std::size_t count = entries_.size(); // dodane w trakcie czekaja do nastepnego kroku
    for (std::size_t i = 0; i < count; ++i)
        if (auto* s = entries_[i].system) s->PreFixedUpdate(dt);
    if (componentsFixedUpdate) componentsFixedUpdate(dt);
    for (std::size_t i = 0; i < count; ++i)
        if (auto* s = entries_[i].system) s->PostFixedUpdate(dt);
    --running_;
    if (running_ == 0 && needsCompact_) Compact();
}

} // namespace wz
