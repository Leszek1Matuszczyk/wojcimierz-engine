// =============================================================================
// Wojcimierz Engine - tests/test_game_loop.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using namespace wz;

namespace {
struct Counters {
    int fixed = 0, update = 0, late = 0, render = 0;
    float lastFixedDt = 0, lastDt = 0;
};

GameLoop MakeLoop(Counters& c, double fixedDt = 0.02, double maxDt = 0.25) {
    GameLoop loop(GameLoop::Settings{fixedDt, maxDt});
    loop.SetCallbacks({[&c](float dt) { ++c.fixed; c.lastFixedDt = dt; },
                       [&c](float dt) { ++c.update; c.lastDt = dt; },
                       [&c](float) { ++c.late; },
                       [&c](float) { ++c.render; }});
    return loop;
}

class GameLoopTest : public ::testing::Test {
protected:
    void SetUp() override { Time::Reset(); }
};
} // namespace

TEST_F(GameLoopTest, FixedStepsMatchElapsedTime) {
    Counters c;
    auto loop = MakeLoop(c);
    for (int i = 0; i < 50; ++i) loop.Tick(0.01); // 0.5 s przy kroku 0.02 s
    EXPECT_EQ(c.fixed, 25);
    EXPECT_EQ(c.update, 50);
    EXPECT_EQ(c.late, 50);
    EXPECT_EQ(c.render, 50);
    EXPECT_FLOAT_EQ(c.lastFixedDt, 0.02f);
    EXPECT_NEAR(Time::time, 0.5, 1e-9);
    EXPECT_NEAR(Time::fixedTime, 0.5, 1e-6);
    EXPECT_EQ(Time::frameCount, 50u);
    EXPECT_EQ(Time::fixedStepCount, 25u);
}

TEST_F(GameLoopTest, LongFrameRunsMultipleFixedStepsAndKeepsRemainder) {
    Counters c;
    auto loop = MakeLoop(c);
    EXPECT_EQ(loop.Tick(0.05), 2);
    EXPECT_NEAR(loop.Accumulator(), 0.01, 1e-6);
    EXPECT_NEAR(loop.Alpha(), 0.5f, 1e-5f);
    EXPECT_EQ(loop.Tick(0.01), 1);
    EXPECT_NEAR(loop.Accumulator(), 0.0, 1e-6);
}

TEST_F(GameLoopTest, ShortFramesMayRunZeroFixedSteps) {
    Counters c;
    auto loop = MakeLoop(c);
    EXPECT_EQ(loop.Tick(0.005), 0);
    EXPECT_EQ(c.fixed, 0);
    EXPECT_EQ(c.update, 1);
}

TEST_F(GameLoopTest, MaximumDeltaTimeClampsSpiralOfDeath) {
    Counters c;
    auto loop = MakeLoop(c, 0.02, 0.1);
    EXPECT_EQ(loop.Tick(5.0), 5); // 5 s przyciete do 0.1 s
    EXPECT_FLOAT_EQ(c.lastDt, 0.1f);
}

TEST_F(GameLoopTest, TimeScaleScalesGameTime) {
    Counters c;
    auto loop = MakeLoop(c);
    Time::timeScale = 0.5f;
    for (int i = 0; i < 10; ++i) loop.Tick(0.02);
    EXPECT_EQ(c.fixed, 5);
    EXPECT_NEAR(Time::time, 0.1, 1e-6);
    EXPECT_NEAR(Time::unscaledTime, 0.2, 1e-6);
    Time::timeScale = 0.0f; // pauza
    loop.Tick(0.02);
    EXPECT_EQ(c.fixed, 5);
}

TEST_F(GameLoopTest, NegativeFrameTimeIsIgnored) {
    Counters c;
    auto loop = MakeLoop(c);
    loop.Tick(-1.0);
    EXPECT_EQ(c.fixed, 0);
    EXPECT_FLOAT_EQ(Time::deltaTime, 0.0f);
}

TEST_F(GameLoopTest, DeltaTimeInsideFixedUpdateIsFixedDeltaTime) {
    float seenInFixed = -1;
    bool flagInFixed = false;
    GameLoop loop(GameLoop::Settings{0.02f, 0.25f});
    loop.SetCallbacks({[&](float) { seenInFixed = Time::deltaTime; flagInFixed = Time::inFixedTimeStep; },
                       nullptr, nullptr, nullptr});
    loop.Tick(0.03);
    EXPECT_FLOAT_EQ(seenInFixed, 0.02f);
    EXPECT_TRUE(flagInFixed);
    EXPECT_FALSE(Time::inFixedTimeStep);
    EXPECT_FLOAT_EQ(Time::deltaTime, 0.03f);
}

// --- Zegar wstrzykiwany ---

TEST_F(GameLoopTest, ManualClockDrivesStepWithoutRealTime) {
    Counters c;
    auto loop = MakeLoop(c);
    ManualClock clock(100.0);
    loop.SetClock(&clock);
    loop.SyncClock();
    for (int i = 0; i < 60; ++i) {
        clock.tick(1.0 / 60.0);
        loop.Step();
    }
    EXPECT_EQ(c.update, 60);
    EXPECT_EQ(c.fixed, 50); // 1 s / 0.02 s
    EXPECT_NEAR(Time::time, 1.0, 1e-6);
}

TEST_F(GameLoopTest, StepWithoutClockAdvanceGivesZeroDelta) {
    Counters c;
    auto loop = MakeLoop(c);
    ManualClock clock;
    loop.SetClock(&clock);
    loop.Step(); // pierwsza klatka: dt = 0
    loop.Step(); // zegar stoi: dt = 0
    EXPECT_EQ(c.update, 2);
    EXPECT_EQ(c.fixed, 0);
    EXPECT_FLOAT_EQ(c.lastDt, 0.0f);
    clock.Tick(0.04);
    EXPECT_EQ(loop.Step(), 2);
}

TEST_F(GameLoopTest, ManualClockIsFullyDeterministic) {
    auto run = [] {
        Time::Reset();
        Counters c;
        auto loop = MakeLoop(c, 1.0 / 60.0);
        ManualClock clock;
        loop.SetClock(&clock);
        loop.SyncClock();
        const double pattern[] = {0.016, 0.017, 0.033, 0.001, 0.050};
        for (int i = 0; i < 100; ++i) { clock.Tick(pattern[i % 5]); loop.Step(); }
        return std::make_pair(c.fixed, Time::time);
    };
    const auto a = run();
    const auto b = run();
    EXPECT_EQ(a.first, b.first);
    EXPECT_DOUBLE_EQ(a.second, b.second);
}

TEST_F(GameLoopTest, SystemClockIsMonotonic) {
    SystemClock clock;
    const double t0 = clock.Now();
    const double t1 = clock.Now();
    EXPECT_GE(t1, t0);
    EXPECT_GE(t0, 0.0);
}

// --- Przypadki brzegowe (issue #1, #2) ---

TEST_F(GameLoopTest, NaNFrameTimeIsTreatedAsZero) {
    Counters c;
    auto loop = MakeLoop(c);
    loop.Tick(std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(std::isnan(Time::time));
    EXPECT_FALSE(std::isnan(loop.Accumulator()));
    EXPECT_FALSE(std::isnan(loop.Alpha()));
    EXPECT_FLOAT_EQ(Time::deltaTime, 0.0f);
    EXPECT_EQ(loop.Tick(0.04), 2);
    EXPECT_NEAR(Time::time, 0.04, 1e-9);
}

TEST_F(GameLoopTest, InfiniteFrameTimeClampsToMaximumDeltaTime) {
    Counters c;
    auto loop = MakeLoop(c, 0.02, 0.25);
    EXPECT_EQ(loop.Tick(std::numeric_limits<double>::infinity()), 12);
    EXPECT_TRUE(std::isfinite(Time::time));
    EXPECT_NEAR(Time::time, 0.25, 1e-6);
}

TEST_F(GameLoopTest, DefaultMaximumDeltaTimeIsQuarterSecond) {
    EXPECT_DOUBLE_EQ(GameLoop().GetSettings().maximumDeltaTime, 0.25);
}

TEST_F(GameLoopTest, NegativeTimeScaleIsClampedToZero) {
    Counters c;
    auto loop = MakeLoop(c);
    loop.Tick(0.1);
    const double before = Time::time;
    Time::timeScale = -1.0f;
    loop.Tick(0.1);
    EXPECT_FLOAT_EQ(Time::timeScale, 0.0f);
    EXPECT_DOUBLE_EQ(Time::time, before);
    EXPECT_GE(loop.Accumulator(), 0.0);
    EXPECT_GE(loop.Alpha(), 0.0f);
    Time::timeScale = std::numeric_limits<float>::quiet_NaN();
    loop.Tick(0.1);
    EXPECT_DOUBLE_EQ(Time::time, before);
}

TEST_F(GameLoopTest, AlphaAlwaysInUnitRange) {
    Counters c;
    auto loop = MakeLoop(c);
    for (double dt : {0.001, 0.013, 0.02, 0.039, 0.25, 0.0, 1e-12, 0.0199999}) {
        loop.Tick(dt);
        EXPECT_GE(loop.Alpha(), 0.0f);
        EXPECT_LT(loop.Alpha(), 1.0f);
    }
}

// Krok staly jest double od konca do konca: godzina symulacji po 1/60 s nie dryfuje.
TEST_F(GameLoopTest, FixedTimeDoesNotDriftOverOneHourOfSixtyHzSteps) {
    Counters c;
    auto loop = MakeLoop(c, 1.0 / 60.0, 0.25);
    constexpr int kSteps = 216000; // 3600 s * 60 Hz
    for (int i = 0; i < kSteps; ++i) loop.Tick(1.0 / 60.0);
    EXPECT_EQ(c.fixed, kSteps);
    EXPECT_EQ(Time::fixedStepCount, static_cast<std::uint64_t>(kSteps));
    EXPECT_DOUBLE_EQ(Time::fixedDeltaTime, 1.0 / 60.0);
    EXPECT_NEAR(Time::fixedTime, 3600.0, 1e-9);
    EXPECT_FLOAT_EQ(c.lastFixedDt, 1.0f / 60.0f); // komponenty dostaja float (API jak w Unity)
}

TEST_F(GameLoopTest, TimeResetRestartsFixedTimeAccumulation) {
    Counters c;
    auto loop = MakeLoop(c, 1.0 / 60.0, 0.25);
    for (int i = 0; i < 1000; ++i) loop.Tick(1.0 / 60.0);
    Time::Reset();
    for (int i = 0; i < 60; ++i) loop.Tick(1.0 / 60.0);
    EXPECT_NEAR(Time::fixedTime, 1.0, 1e-12);
    EXPECT_DOUBLE_EQ(Time::maximumDeltaTime, 0.25);
}
