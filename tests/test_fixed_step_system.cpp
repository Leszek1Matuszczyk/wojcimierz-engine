// =============================================================================
// Wojcimierz Engine - tests/test_fixed_step_system.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/FixedStepSystem.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace wz;

namespace {

std::vector<std::string>* gLog = nullptr;

class LogComponent : public Component {
public:
    void FixedUpdate(float) override { gLog->push_back("component.FixedUpdate"); }
    void Update(float) override { gLog->push_back("component.Update"); }
};

// Atrapa "fizyki": nie zalezy od Jolt, tylko loguje kolejnosc.
class FakePhysics : public IFixedStepSystem {
public:
    explicit FakePhysics(std::string name) : name_(std::move(name)) {}
    void PreFixedUpdate(float dt) override { gLog->push_back(name_ + ".Pre"); lastDt = dt; }
    void PostFixedUpdate(float) override { gLog->push_back(name_ + ".Post"); ++steps; }
    float lastDt = 0.0f;
    int steps = 0;

private:
    std::string name_;
};

class FixedStepSystemTest : public ::testing::Test {
protected:
    void SetUp() override { Time::Reset(); log.clear(); gLog = &log; }
    void TearDown() override { gLog = nullptr; Time::Reset(); }

    GameLoop MakeLoop(Scene& scene, FixedStepPipeline& pipeline) {
        GameLoop loop(GameLoop::Settings{0.02f, 0.25f});
        GameLoop::Callbacks cb;
        cb.fixedUpdate = [&](float dt) { pipeline.Run(dt, [&](float d) { scene.FixedUpdate(d); }); };
        cb.update = [&](float dt) { scene.Update(dt); };
        cb.lateUpdate = [&](float dt) { scene.LateUpdate(dt); };
        loop.SetCallbacks(cb);
        return loop;
    }
    std::vector<std::string> log;
};

} // namespace

TEST_F(FixedStepSystemTest, SystemsWrapComponentFixedUpdateInUnityOrder) {
    Scene scene;
    scene.CreateGameObject("A").AddComponent<LogComponent>();
    FixedStepPipeline pipeline;
    FakePhysics physics("physics");
    pipeline.Add(&physics);
    GameLoop loop = MakeLoop(scene, pipeline);

    EXPECT_EQ(loop.Tick(0.04), 2);
    const std::vector<std::string> expected = {
        "physics.Pre", "component.FixedUpdate", "physics.Post",
        "physics.Pre", "component.FixedUpdate", "physics.Post",
        "component.Update"};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(physics.steps, 2);
    EXPECT_FLOAT_EQ(physics.lastDt, 0.02f);
}

TEST_F(FixedStepSystemTest, NoFixedStepMeansNoSystemCall) {
    Scene scene;
    FixedStepPipeline pipeline;
    FakePhysics physics("physics");
    pipeline.Add(&physics);
    GameLoop loop = MakeLoop(scene, pipeline);
    loop.Tick(0.01);
    EXPECT_EQ(physics.steps, 0);
}

TEST_F(FixedStepSystemTest, OrderThenInsertionOrder) {
    FixedStepPipeline pipeline;
    FakePhysics a("a"), b("b"), c("c");
    pipeline.Add(&a, 10);
    pipeline.Add(&b, -5);
    pipeline.Add(&c, 10);
    pipeline.Add(&c, 0); // duplikat ignorowany
    EXPECT_EQ(pipeline.Size(), 3u);
    pipeline.Run(0.02f, [](float) { gLog->push_back("components"); });
    const std::vector<std::string> expected = {"b.Pre", "a.Pre", "c.Pre", "components", "b.Post", "a.Post", "c.Post"};
    EXPECT_EQ(log, expected);
}

namespace {
class SelfRemoving : public IFixedStepSystem {
public:
    FixedStepPipeline* pipeline = nullptr;
    IFixedStepSystem* toAdd = nullptr;
    void PreFixedUpdate(float) override {
        gLog->push_back("self.Pre");
        pipeline->Remove(this);
        if (toAdd) pipeline->Add(toAdd);
    }
    void PostFixedUpdate(float) override { gLog->push_back("self.Post"); }
};
} // namespace

TEST_F(FixedStepSystemTest, RemoveAndAddDuringRunAreSafe) {
    FixedStepPipeline pipeline;
    SelfRemoving self;
    FakePhysics late("late");
    self.pipeline = &pipeline;
    self.toAdd = &late;
    pipeline.Add(&self);
    pipeline.Run(0.02f, nullptr);
    EXPECT_EQ(log, (std::vector<std::string>{"self.Pre"})); // usuniety -> brak Post, dodany -> od nast. kroku
    log.clear();
    pipeline.Run(0.02f, nullptr);
    EXPECT_EQ(log, (std::vector<std::string>{"late.Pre", "late.Post"}));
    EXPECT_FALSE(pipeline.Contains(&self));
    EXPECT_TRUE(pipeline.Remove(&late));
    EXPECT_FALSE(pipeline.Remove(&late));
    EXPECT_EQ(pipeline.Size(), 0u);
}
