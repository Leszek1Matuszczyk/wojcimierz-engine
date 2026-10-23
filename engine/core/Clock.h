#pragma once
#include <chrono>

namespace wz {

// Zrodlo czasu wstrzykiwane do petli gry / Engine.
// Dzieki temu testy steruja klatkami recznie, bez prawdziwego uplywu czasu.
class IClock {
public:
    virtual ~IClock() = default;
    // Monotoniczny czas w sekundach od dowolnego punktu odniesienia.
    virtual double Now() const = 0;
};

// Domyslny zegar: std::chrono::steady_clock.
class SystemClock final : public IClock {
public:
    SystemClock() : start_(std::chrono::steady_clock::now()) {}
    double Now() const override {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

// Zegar reczny: czas plynie tylko po Tick(dt). Do testow i trybu headless.
class ManualClock final : public IClock {
public:
    explicit ManualClock(double start = 0.0) : now_(start) {}
    double Now() const override { return now_; }
    void Tick(double dt) { now_ += dt; }
    void tick(double dt) { Tick(dt); } // alias zgodny z ustaleniami zespolu
    void Set(double t) { now_ = t; }

private:
    double now_;
};

} // namespace wz
