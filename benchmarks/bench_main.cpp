// =============================================================================
// Wojcimierz Engine - benchmarks/bench_main.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
// Mikro-benchmarki rdzenia (bez GPU). Wynik: tabela + plik JSON (--json plik).
//   wz_benchmarks [--quick] [--json wyniki.json]
// JSON (schema 2): {"schema": 2, "frame": {"count", "avg_ms", "p95_ms"}, "peak_rss_mb", "benchmarks": [...]}
//   frame - czas CPU klatki petli gry (FixedUpdate + Update + LateUpdate sceny, bez GPU),
//   peak_rss_mb - szczytowe zuzycie pamieci procesu. Porownanie z baseline: scripts/compare_benchmarks.py.
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "scene/Scene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

using namespace wz;

namespace {

struct Result {
    std::string name;
    double nsPerOp;
    long long ops;
};

Result Measure(const std::string& name, long long ops, const std::function<void()>& body) {
    const auto t0 = std::chrono::steady_clock::now();
    body();
    const auto t1 = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return {name, ns / static_cast<double>(ops), ops};
}

class Rotator : public Component {
public:
    void Update(float dt) override { transform().Rotate({0.0f, 30.0f * dt, 0.0f}); }
};

class FixedMover : public Component {
public:
    void FixedUpdate(float dt) override { transform().Translate({0.0f, 0.1f * dt, 0.0f}); }
};

volatile float g_sink = 0.0f; // zapobiega wycieciu obliczen przez optymalizator

// Szczytowe RSS procesu w MB (0 gdy platforma nie wspierana).
double PeakRssMb() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    return 0.0;
#else
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) != 0) return 0.0;
#if defined(__APPLE__)
    return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0); // bajty
#else
    return static_cast<double>(ru.ru_maxrss) / 1024.0; // KiB
#endif
#endif
}

struct FrameStats {
    long long count = 0;
    double avgMs = 0.0;
    double p95Ms = 0.0;
};

// p95 metoda "nearest rank": najmniejsza wartosc, od ktorej >= 95% probek jest nie wieksze.
FrameStats ComputeFrameStats(std::vector<double> ms) {
    FrameStats st;
    st.count = static_cast<long long>(ms.size());
    if (ms.empty()) return st;
    double sum = 0.0;
    for (double v : ms) sum += v;
    st.avgMs = sum / static_cast<double>(ms.size());
    std::sort(ms.begin(), ms.end());
    const auto rank = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(ms.size())));
    st.p95Ms = ms[std::max<std::size_t>(rank, 1) - 1];
    return st;
}

} // namespace

int main(int argc, char** argv) {
    bool quick = false;
    const char* jsonPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--quick")) quick = true;
        else if (!std::strcmp(argv[i], "--json") && i + 1 < argc) jsonPath = argv[++i];
    }
    const int scale = quick ? 1 : 20;
    std::vector<Result> results;

    // 1) Macierz swiatowa w glebokiej hierarchii: zmiana korzenia + odczyt liscia.
    {
        Scene s;
        const int depth = 64;
        Transform* prev = nullptr;
        Transform* root = nullptr;
        for (int i = 0; i < depth; ++i) {
            auto& t = s.CreateGameObject().transform();
            t.SetLocalPosition({0.1f, 0.0f, 0.0f});
            if (prev) t.SetParent(prev, false); else root = &t;
            prev = &t;
        }
        Transform* leaf = prev;
        const long long ops = 2000LL * scale;
        results.push_back(Measure("hierarchy64_root_move_leaf_read", ops, [&] {
            for (long long i = 0; i < ops; ++i) {
                root->SetLocalPosition({static_cast<float>(i % 7), 0.0f, 0.0f});
                g_sink = g_sink + leaf->GetLocalToWorldMatrix()[3][0];
            }
        }));
        const long long ops2 = 200000LL * scale;
        results.push_back(Measure("hierarchy64_cached_leaf_read", ops2, [&] {
            for (long long i = 0; i < ops2; ++i) g_sink = g_sink + leaf->GetLocalToWorldMatrix()[3][0];
        }));
    }

    // 2) Scene::Update na 10 000 komponentach.
    {
        Scene s;
        const int count = 10000;
        for (int i = 0; i < count; ++i) s.CreateGameObject().AddComponent<Rotator>();
        const int frames = 5 * scale;
        auto r = Measure("scene_update_10k_components_per_component", static_cast<long long>(frames) * count, [&] {
            for (int f = 0; f < frames; ++f) { s.Update(1.0f / 60.0f); s.LateUpdate(1.0f / 60.0f); }
        });
        results.push_back(r);
    }

    // 3) Narzut petli gry (ManualClock, puste callbacki).
    {
        Time::Reset();
        ManualClock clock;
        GameLoop loop;
        loop.SetClock(&clock);
        int fixed = 0;
        loop.SetCallbacks({[&](float) { ++fixed; }, [](float) {}, [](float) {}, nullptr});
        loop.SyncClock();
        const long long ops = 100000LL * scale;
        results.push_back(Measure("game_loop_step_overhead", ops, [&] {
            for (long long i = 0; i < ops; ++i) { clock.Tick(1.0 / 144.0); loop.Step(); }
        }));
        g_sink = g_sink + static_cast<float>(fixed);
    }

    // 4) Czas CPU klatki: scena 2000 obiektow (Update + FixedUpdate) przez GameLoop z krokiem 1/60 s.
    FrameStats frame;
    {
        Time::Reset();
        Scene s;
        for (int i = 0; i < 2000; ++i) {
            auto& go = s.CreateGameObject();
            go.AddComponent<Rotator>();
            go.AddComponent<FixedMover>();
        }
        ManualClock clock;
        GameLoop loop;
        loop.SetClock(&clock);
        loop.SetCallbacks({[&](float dt) { s.FixedUpdate(dt); }, [&](float dt) { s.Update(dt); },
                           [&](float dt) { s.LateUpdate(dt); }, nullptr});
        loop.SyncClock();
        const int warmup = 10;
        const int frames = 30 * scale; // quick: 30, pelny: 600
        std::vector<double> ms;
        ms.reserve(static_cast<std::size_t>(frames));
        for (int f = 0; f < warmup + frames; ++f) {
            clock.Tick(1.0 / 60.0);
            const auto t0 = std::chrono::steady_clock::now();
            loop.Step();
            const auto t1 = std::chrono::steady_clock::now();
            if (f >= warmup) ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        frame = ComputeFrameStats(std::move(ms));
    }
    const double peakRssMb = PeakRssMb();

    std::printf("%-45s %14s %12s\n", "benchmark", "ns/op", "ops");
    for (const auto& r : results) std::printf("%-45s %14.2f %12lld\n", r.name.c_str(), r.nsPerOp, r.ops);
    std::printf("\nklatka CPU (2000 obiektow): %lld klatek, avg %.3f ms, p95 %.3f ms\n", frame.count, frame.avgMs,
                frame.p95Ms);
    std::printf("szczytowe RSS: %.1f MB\n", peakRssMb);

    if (jsonPath) {
        FILE* f = std::fopen(jsonPath, "w");
        if (!f) {
            std::fprintf(stderr, "Nie moge zapisac %s\n", jsonPath);
            return 1;
        }
        std::fprintf(f, "{\n  \"schema\": 2,\n  \"quick\": %s,\n", quick ? "true" : "false");
        std::fprintf(f, "  \"frame\": {\"count\": %lld, \"avg_ms\": %.4f, \"p95_ms\": %.4f},\n", frame.count,
                     frame.avgMs, frame.p95Ms);
        std::fprintf(f, "  \"peak_rss_mb\": %.2f,\n  \"benchmarks\": [\n", peakRssMb);
        for (std::size_t i = 0; i < results.size(); ++i)
            std::fprintf(f, "    {\"name\": \"%s\", \"ns_per_op\": %.3f, \"ops\": %lld}%s\n", results[i].name.c_str(),
                         results[i].nsPerOp, results[i].ops, i + 1 < results.size() ? "," : "");
        std::fprintf(f, "  ]\n}\n");
        std::fclose(f);
    }
    return 0;
}
