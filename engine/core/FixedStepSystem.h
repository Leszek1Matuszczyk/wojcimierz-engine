// =============================================================================
// Wojcimierz Engine - engine/core/FixedStepSystem.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include <functional>
#include <vector>

namespace wz {

// System wykonywany w kazdym kroku stalym petli gry (np. fizyka).
// Kolejnosc jak w Unity w jednym kroku stalym:
//   1. PreFixedUpdate  systemow (np. synchronizacja Transform -> fizyka),
//   2. FixedUpdate     komponentow (Scene::FixedUpdate),
//   3. PostFixedUpdate systemow (np. PhysicsWorld::Step + zdarzenia kolizji).
// Rdzen nie zalezy od zadnego silnika fizyki - backend (np. Jolt) implementuje ten interfejs.
class IFixedStepSystem {
public:
    virtual ~IFixedStepSystem() = default;
    virtual void PreFixedUpdate(float /*fixedDeltaTime*/) {}
    virtual void PostFixedUpdate(float /*fixedDeltaTime*/) {}
};

// Uporzadkowana lista systemow kroku stalego. Nie posiada systemow (wskazniki nie-wlascicielskie).
// Systemy z mniejszym `order` wykonuja sie wczesniej (przy rownych - kolejnosc dodania).
// Dodawanie/usuwanie w trakcie Run() jest bezpieczne: dodane dzialaja od nastepnego kroku,
// usuniete nie sa juz wolane.
class FixedStepPipeline {
public:
    void Add(IFixedStepSystem* system, int order = 0);
    bool Remove(IFixedStepSystem* system);
    bool Contains(const IFixedStepSystem* system) const;
    std::size_t Size() const;

    // Jeden krok staly: Pre (systemy) -> componentsFixedUpdate(dt) -> Post (systemy).
    void Run(float fixedDeltaTime, const std::function<void(float)>& componentsFixedUpdate);

private:
    struct Entry {
        IFixedStepSystem* system;
        int order;
        unsigned long long seq;
    };
    void Compact();

    std::vector<Entry> entries_;
    unsigned long long nextSeq_ = 0;
    int running_ = 0;
    bool needsCompact_ = false;
};

} // namespace wz
