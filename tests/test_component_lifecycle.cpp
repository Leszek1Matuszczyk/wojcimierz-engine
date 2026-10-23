// =============================================================================
// Wojcimierz Engine - tests/test_component_lifecycle.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace wz;

namespace {
std::vector<std::string>* g_log = nullptr;

class Recorder : public Component {
public:
    explicit Recorder(std::string id) : id_(std::move(id)) {}
    void Awake() override { Log("Awake"); }
    void Start() override { Log("Start"); }
    void FixedUpdate(float) override { Log("FixedUpdate"); }
    void Update(float) override { Log("Update"); }
    void LateUpdate(float) override { Log("LateUpdate"); }
    void OnDestroy() override { Log("OnDestroy"); }

private:
    void Log(const char* what) { if (g_log) g_log->push_back(id_ + ":" + what); }
    std::string id_;
};

struct LogScope {
    std::vector<std::string> log;
    LogScope() { g_log = &log; Time::Reset(); }
    ~LogScope() { g_log = nullptr; }
};
} // namespace

TEST(ComponentLifecycle, AwakeImmediatelyStartBeforeFirstUpdate) {
    LogScope ls;
    Scene s;
    auto& go = s.CreateGameObject();
    auto& r = go.AddComponent<Recorder>("A");
    EXPECT_EQ(ls.log, (std::vector<std::string>{"A:Awake"}));
    EXPECT_FALSE(r.HasStarted());
    s.Update(0.016f);
    EXPECT_TRUE(r.HasStarted());
    EXPECT_EQ(ls.log, (std::vector<std::string>{"A:Awake", "A:Start", "A:Update"}));
}

TEST(ComponentLifecycle, UnityOrderWithinFrameViaGameLoop) {
    LogScope ls;
    Scene s;
    s.CreateGameObject().AddComponent<Recorder>("A");
    ManualClock clock;
    GameLoop loop(GameLoop::Settings{0.02f, 0.5f});
    loop.SetClock(&clock);
    loop.SetCallbacks({[&](float dt) { s.FixedUpdate(dt); }, [&](float dt) { s.Update(dt); },
                       [&](float dt) { s.LateUpdate(dt); }, nullptr});
    loop.SyncClock();
    clock.Tick(0.04); // dwa kroki stale
    loop.Step();
    EXPECT_EQ(ls.log, (std::vector<std::string>{"A:Awake", "A:Start", "A:FixedUpdate", "A:FixedUpdate", "A:Update",
                                                "A:LateUpdate"}));
}

TEST(ComponentLifecycle, StartCalledOnlyOnce) {
    LogScope ls;
    Scene s;
    s.CreateGameObject().AddComponent<Recorder>("A");
    for (int i = 0; i < 3; ++i) { s.Update(0.1f); s.LateUpdate(0.1f); }
    EXPECT_EQ(std::count(ls.log.begin(), ls.log.end(), "A:Start"), 1);
    EXPECT_EQ(std::count(ls.log.begin(), ls.log.end(), "A:Update"), 3);
}

TEST(ComponentLifecycle, DisabledComponentGetsNoUpdatesAndDefersStart) {
    LogScope ls;
    Scene s;
    auto& r = s.CreateGameObject().AddComponent<Recorder>("A");
    r.enabled = false;
    s.Update(0.1f);
    EXPECT_FALSE(r.HasStarted());
    EXPECT_EQ(ls.log, (std::vector<std::string>{"A:Awake"}));
    r.enabled = true;
    s.Update(0.1f);
    EXPECT_EQ(ls.log, (std::vector<std::string>{"A:Awake", "A:Start", "A:Update"}));
}

TEST(ComponentLifecycle, InactiveGameObjectGetsNoUpdates) {
    LogScope ls;
    Scene s;
    auto& go = s.CreateGameObject();
    go.AddComponent<Recorder>("A");
    go.SetActive(false);
    s.FixedUpdate(0.02f);
    s.Update(0.1f);
    s.LateUpdate(0.1f);
    EXPECT_EQ(ls.log, (std::vector<std::string>{"A:Awake"}));
}

TEST(ComponentLifecycle, DestroyIsDeferredToEndOfFrame) {
    LogScope ls;
    Scene s;
    auto& go = s.CreateGameObject("X");
    go.AddComponent<Recorder>("A");
    s.Update(0.1f);
    s.Destroy(go);
    EXPECT_EQ(s.Find("X"), nullptr); // oczekuje na zniszczenie - Find go juz nie zwraca
    EXPECT_EQ(s.GetGameObjectCount(), 1u);
    s.LateUpdate(0.1f); // obiekt oczekujacy na zniszczenie nie dostaje juz LateUpdate
    EXPECT_EQ(s.GetGameObjectCount(), 0u);
    EXPECT_EQ(ls.log.back(), "A:OnDestroy");
    EXPECT_EQ(std::count(ls.log.begin(), ls.log.end(), "A:LateUpdate"), 0);
}

TEST(ComponentLifecycle, OnDestroyCalledWhenSceneIsDestroyed) {
    LogScope ls;
    {
        Scene s;
        auto& go = s.CreateGameObject();
        go.AddComponent<Recorder>("A");
        go.AddComponent<Recorder>("B");
    }
    ASSERT_GE(ls.log.size(), 2u);
    // Odwrotna kolejnosc dodania - jak destruktory.
    EXPECT_EQ(ls.log[ls.log.size() - 2], "B:OnDestroy");
    EXPECT_EQ(ls.log[ls.log.size() - 1], "A:OnDestroy");
}

TEST(ComponentLifecycle, ComponentsAddedDuringUpdateStartNextFrame) {
    LogScope ls;
    class Spawner : public Component {
    public:
        void Update(float) override {
            if (!spawned) { spawned = true; gameObject().AddComponent<Recorder>("Child"); }
        }
        bool spawned = false;
    };
    Scene s;
    s.CreateGameObject().AddComponent<Spawner>();
    s.Update(0.1f);
    EXPECT_EQ(ls.log, (std::vector<std::string>{"Child:Awake"}));
    s.Update(0.1f);
    EXPECT_EQ(ls.log, (std::vector<std::string>{"Child:Awake", "Child:Start", "Child:Update"}));
}

TEST(ComponentLifecycle, GetComponentFindsByType) {
    Scene s;
    auto& go = s.CreateGameObject();
    EXPECT_EQ(go.GetComponent<Transform>(), &go.transform());
    EXPECT_EQ(go.GetComponent<Recorder>(), nullptr);
    auto& r = go.AddComponent<Recorder>("A");
    EXPECT_EQ(go.GetComponent<Recorder>(), &r);
    EXPECT_EQ(&r.transform(), &go.transform());
    EXPECT_EQ(&r.gameObject(), &go);
    go.AddComponent<Recorder>("B");
    EXPECT_EQ(go.GetComponents<Recorder>().size(), 2u);
}
