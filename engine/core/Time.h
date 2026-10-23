#pragma once
#include <cstdint>

namespace wz {

// Odpowiednik UnityEngine.Time. Wartosci ustawia petla gry (GameLoop).
struct Time {
    static inline double time = 0.0;            // czas gry od startu [s] (skalowany)
    static inline double unscaledTime = 0.0;    // czas bez timeScale [s]
    static inline float deltaTime = 0.0f;       // czas ostatniej klatki [s] (w FixedUpdate = fixedDeltaTime)
    static inline float unscaledDeltaTime = 0.0f;
    static inline float fixedDeltaTime = 1.0f / 60.0f;
    static inline float timeScale = 1.0f;
    static inline double fixedTime = 0.0;       // czas symulacji fizyki [s]
    static inline std::uint64_t frameCount = 0;
    static inline std::uint64_t fixedStepCount = 0;
    static inline bool inFixedTimeStep = false;

    // Przywraca wartosci domyslne (np. miedzy testami albo scenami testowymi).
    static void Reset();
};

} // namespace wz
