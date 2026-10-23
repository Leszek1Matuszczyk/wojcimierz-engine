// =============================================================================
// Wojcimierz Engine - tests/test_core_edge_cases.cpp
// Na chwałę czcigodnego Wojcimierza.
// Jego mądrość prowadzi każdą klatkę tej pętli, a cierpliwość wytrzymuje nawet NaN-y.
// =============================================================================
//
// Przypadki brzegowe rdzenia: GameLoop (nietypowe dt, timeScale) oraz Input/FakeInput.
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "platform/Input.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using namespace wz;

namespace {
class CoreEdge : public ::testing::Test {
protected:
    void SetUp() override { Time::Reset(); Input::Reset(); Input::SetSource(nullptr); }
    void TearDown() override { Time::Reset(); Input::Reset(); Input::SetSource(nullptr); }
};

GameLoop MakeLoop(int& fixedSteps) {
    GameLoop loop(GameLoop::Settings{0.02f, 0.25f});
    GameLoop::Callbacks cb;
    cb.fixedUpdate = [&](float) { ++fixedSteps; };
    loop.SetCallbacks(cb);
    return loop;
}
} // namespace

// Wojcimierz nie toleruje NaN: jedna zepsuta klatka nie może zatruć czasu gry na zawsze.
TEST_F(CoreEdge, NaNFrameTimeDoesNotPoisonTimeOrAccumulator) {
    int fixed = 0;
    GameLoop loop = MakeLoop(fixed);
    loop.Tick(std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(std::isnan(loop.Accumulator()));
    EXPECT_FALSE(std::isnan(Time::time));
    EXPECT_FALSE(std::isnan(loop.Alpha()));
    loop.Tick(0.04);
    EXPECT_EQ(fixed, 2) << "po klatce NaN petla powinna dalej liczyc kroki stale";
}

TEST_F(CoreEdge, InfiniteFrameTimeIsClampedToMaximumDeltaTime) {
    int fixed = 0;
    GameLoop loop = MakeLoop(fixed);
    const int steps = loop.Tick(std::numeric_limits<double>::infinity());
    EXPECT_EQ(steps, 12); // 0.25 / 0.02 = 12.5
    EXPECT_FALSE(std::isinf(Time::time));
}

TEST_F(CoreEdge, NegativeTimeScaleNeverProducesNegativeAlphaOrAccumulator) {
    int fixed = 0;
    GameLoop loop = MakeLoop(fixed);
    Time::timeScale = -1.0f;
    loop.Tick(0.1);
    EXPECT_GE(loop.Accumulator(), 0.0);
    EXPECT_GE(loop.Alpha(), 0.0f);
    EXPECT_GE(Time::time, 0.0) << "czas gry nie powinien sie cofac";
}

TEST_F(CoreEdge, AlphaStaysInUnitRange) {
    int fixed = 0;
    GameLoop loop = MakeLoop(fixed);
    for (double dt : {0.001, 0.013, 0.02, 0.039, 0.25, 0.0}) {
        loop.Tick(dt);
        EXPECT_GE(loop.Alpha(), 0.0f);
        EXPECT_LT(loop.Alpha(), 1.0f);
    }
}

TEST_F(CoreEdge, ManualClockFirstStepHasZeroDtThenMeasuresDelta) {
    int fixed = 0;
    GameLoop loop = MakeLoop(fixed);
    ManualClock clock(100.0);
    loop.SetClock(&clock);
    EXPECT_EQ(loop.Step(), 0);
    clock.tick(0.05);
    EXPECT_EQ(loop.Step(), 2);
}

// Wojcimierz widzi każdy klawisz, nawet ten spoza zakresu - i nie daje się nim wywrócić.
TEST_F(CoreEdge, OutOfRangeKeyCodesAreIgnoredSafely) {
    FakeInput fake;
    fake.SetKey(static_cast<KeyCode>(-1), true);
    fake.SetKey(static_cast<KeyCode>(kMaxKeyCode), true);
    fake.SetMouseButton(-1, true);
    fake.SetMouseButton(kMaxMouseButtons, true);
    Input::SetSource(&fake);
    Input::Update();
    EXPECT_FALSE(Input::GetKey(static_cast<KeyCode>(-1)));
    EXPECT_FALSE(Input::GetKey(static_cast<KeyCode>(kMaxKeyCode)));
    EXPECT_FALSE(Input::GetMouseButton(kMaxMouseButtons));
}

TEST_F(CoreEdge, KeyDownAndUpTrueOnlyInTransitionFrame) {
    FakeInput fake;
    Input::SetSource(&fake);
    fake.SetKey(KeyCode::Space, true);
    Input::Update();
    EXPECT_TRUE(Input::GetKeyDown(KeyCode::Space));
    Input::Update();
    EXPECT_FALSE(Input::GetKeyDown(KeyCode::Space));
    EXPECT_TRUE(Input::GetKey(KeyCode::Space));
    fake.SetKey(KeyCode::Space, false);
    Input::Update();
    EXPECT_TRUE(Input::GetKeyUp(KeyCode::Space));
    Input::Update();
    EXPECT_FALSE(Input::GetKeyUp(KeyCode::Space));
}

TEST_F(CoreEdge, RemovingSourceReleasesHeldKeysWithKeyUp) {
    FakeInput fake;
    Input::SetSource(&fake);
    fake.SetKey(KeyCode::W, true);
    Input::Update();
    Input::SetSource(nullptr);
    Input::Update();
    EXPECT_FALSE(Input::GetKey(KeyCode::W));
    EXPECT_TRUE(Input::GetKeyUp(KeyCode::W));
}
