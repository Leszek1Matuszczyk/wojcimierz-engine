// =============================================================================
// Wojcimierz Engine - benchmarks/bench_main.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
// Mikro-benchmarki rdzenia (bez GPU). Wynik: tabela + plik JSON (--json plik).
//   wz_benchmarks [--quick] [--json wyniki.json]
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "scene/Scene.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

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

volatile float g_sink = 0.0f; // zapobiega wycieciu obliczen przez optymalizator

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

    std::printf("%-45s %14s %12s\n", "benchmark", "ns/op", "ops");
    for (const auto& r : results) std::printf("%-45s %14.2f %12lld\n", r.name.c_str(), r.nsPerOp, r.ops);

    if (jsonPath) {
        if (FILE* f = std::fopen(jsonPath, "w")) {
            std::fprintf(f, "[\n");
            for (std::size_t i = 0; i < results.size(); ++i)
                std::fprintf(f, "  {\"name\": \"%s\", \"ns_per_op\": %.3f, \"ops\": %lld}%s\n", results[i].name.c_str(),
                             results[i].nsPerOp, results[i].ops, i + 1 < results.size() ? "," : "");
            std::fprintf(f, "]\n");
            std::fclose(f);
        }
    }
    return 0;
}
