// =============================================================================
// Wojcimierz Engine - engine/core/FixedStepSystem.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include "core/SystemList.h"

#include <functional>

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

// Uporzadkowana lista systemow kroku stalego (wskazniki nie-wlascicielskie, patrz SystemList).
class FixedStepPipeline {
public:
    void Add(IFixedStepSystem* system, int order = 0) { systems_.Add(system, order); }
    bool Remove(IFixedStepSystem* system) { return systems_.Remove(system); }
    bool Contains(const IFixedStepSystem* system) const { return systems_.Contains(system); }
    std::size_t Size() const { return systems_.Size(); }

    // Jeden krok staly: Pre (systemy) -> componentsFixedUpdate(dt) -> Post (systemy).
    void Run(float fixedDeltaTime, const std::function<void(float)>& componentsFixedUpdate);

private:
    SystemList<IFixedStepSystem> systems_;
};

// System wykonywany raz na klatke PO LateUpdate komponentow (i po zniszczeniu obiektow
// z Scene::Destroy), przed renderem - np. AudioSystem::Update() (pozycje zrodel/sluchacza
// sa juz ostateczne). Niezalezny od backendu (miniaudio itp.).
class ILateUpdateSystem {
public:
    virtual ~ILateUpdateSystem() = default;
    virtual void OnLateUpdate(float deltaTime) = 0;
};

class LateUpdatePipeline {
public:
    void Add(ILateUpdateSystem* system, int order = 0) { systems_.Add(system, order); }
    bool Remove(ILateUpdateSystem* system) { return systems_.Remove(system); }
    bool Contains(const ILateUpdateSystem* system) const { return systems_.Contains(system); }
    std::size_t Size() const { return systems_.Size(); }

    // componentsLateUpdate(dt) (np. Scene::LateUpdate), potem OnLateUpdate systemow.
    void Run(float deltaTime, const std::function<void(float)>& componentsLateUpdate);

private:
    SystemList<ILateUpdateSystem> systems_;
};

// Adapter dla systemow bez wlasnego interfejsu, np.:
//   LateUpdateCallback audioHook([&](float) { audio.Update(); });
//   engine.AddLateUpdateSystem(&audioHook);
class LateUpdateCallback final : public ILateUpdateSystem {
public:
    explicit LateUpdateCallback(std::function<void(float)> fn) : fn_(std::move(fn)) {}
    void OnLateUpdate(float dt) override {
        if (fn_) fn_(dt);
    }

private:
    std::function<void(float)> fn_;
};

} // namespace wz
