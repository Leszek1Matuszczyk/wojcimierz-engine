// =============================================================================
// Wojcimierz Engine - tests/test_input.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "core/Clock.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include "platform/Input.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>

using namespace wz;

namespace {
class InputTest : public ::testing::Test {
protected:
    void SetUp() override {
        Time::Reset();
        Input::Reset();
        Input::SetSource(&fake);
    }
    void TearDown() override {
        Input::SetSource(nullptr);
        Input::Reset();
    }
    FakeInput fake;
};

// Przykladowy gameplay: ruch gracza strzalkami/WASD, skok na Space (tylko w klatce wcisniecia).
class PlayerController : public Component {
public:
    float speed = 2.0f;
    int jumps = 0;
    void Update(float dt) override {
        glm::vec3 dir(0.0f);
        if (Input::GetKey(KeyCode::D) || Input::GetKey(KeyCode::RightArrow)) dir.x += 1;
        if (Input::GetKey(KeyCode::A) || Input::GetKey(KeyCode::LeftArrow)) dir.x -= 1;
        if (Input::GetKey(KeyCode::W)) dir.z -= 1;
        if (Input::GetKey(KeyCode::S)) dir.z += 1;
        transform().Translate(dir * speed * dt, true);
        if (Input::GetKeyDown(KeyCode::Space)) ++jumps;
    }
};
} // namespace

TEST_F(InputTest, GetKeyReflectsSourceAfterUpdate) {
    EXPECT_FALSE(Input::GetKey(KeyCode::W));
    fake.SetKey(KeyCode::W, true);
    EXPECT_FALSE(Input::GetKey(KeyCode::W)); // migawka dopiero po Update
    Input::Update();
    EXPECT_TRUE(Input::GetKey(KeyCode::W));
}

TEST_F(InputTest, KeyDownAndUpAreSingleFrameEdges) {
    fake.SetKey(KeyCode::Space, true);
    Input::Update();
    EXPECT_TRUE(Input::GetKeyDown(KeyCode::Space));
    EXPECT_TRUE(Input::GetKey(KeyCode::Space));
    Input::Update(); // nadal wcisniety
    EXPECT_FALSE(Input::GetKeyDown(KeyCode::Space));
    EXPECT_TRUE(Input::GetKey(KeyCode::Space));
    fake.SetKey(KeyCode::Space, false);
    Input::Update();
    EXPECT_TRUE(Input::GetKeyUp(KeyCode::Space));
    EXPECT_FALSE(Input::GetKey(KeyCode::Space));
    Input::Update();
    EXPECT_FALSE(Input::GetKeyUp(KeyCode::Space));
}

TEST_F(InputTest, MouseButtonsAndPosition) {
    fake.SetMouseButton(0, true);
    fake.SetMousePosition({120.0f, 45.5f});
    Input::Update();
    EXPECT_TRUE(Input::GetMouseButton(0));
    EXPECT_TRUE(Input::GetMouseButtonDown(0));
    EXPECT_FALSE(Input::GetMouseButton(1));
    EXPECT_FLOAT_EQ(Input::MousePosition().x, 120.0f);
    EXPECT_FLOAT_EQ(Input::MousePosition().y, 45.5f);
    fake.SetMouseButton(0, false);
    Input::Update();
    EXPECT_TRUE(Input::GetMouseButtonUp(0));
}

TEST_F(InputTest, InvalidCodesAreSafe) {
    fake.SetKey(static_cast<KeyCode>(-5), true);
    fake.SetKey(static_cast<KeyCode>(100000), true);
    fake.SetMouseButton(99, true);
    Input::Update();
    EXPECT_FALSE(Input::GetKey(static_cast<KeyCode>(-5)));
    EXPECT_FALSE(Input::GetMouseButton(99));
}

TEST_F(InputTest, NoSourceMeansNothingPressed) {
    fake.SetKey(KeyCode::A, true);
    Input::Update();
    EXPECT_TRUE(Input::GetKey(KeyCode::A));
    Input::SetSource(nullptr);
    Input::Update();
    EXPECT_FALSE(Input::GetKey(KeyCode::A));
}

TEST_F(InputTest, GameplayDrivenByFakeInputAndManualClock) {
    Scene scene;
    auto& player = scene.CreateGameObject("Player").AddComponent<PlayerController>();
    ManualClock clock;
    GameLoop loop(GameLoop::Settings{1.0f / 60.0f, 0.25f});
    loop.SetClock(&clock);
    loop.SetCallbacks({[&](float dt) { scene.FixedUpdate(dt); }, [&](float dt) { scene.Update(dt); },
                       [&](float dt) { scene.LateUpdate(dt); }, nullptr});
    loop.SyncClock();
    auto frame = [&] {
        clock.Tick(0.1);
        Input::Update();
        loop.Step();
    };

    fake.SetKey(KeyCode::D, true);
    for (int i = 0; i < 5; ++i) frame(); // 0.5 s w prawo przy 2 j/s
    EXPECT_NEAR(player.transform().GetPosition().x, 1.0f, 1e-4f);

    fake.SetKey(KeyCode::D, false);
    fake.SetKey(KeyCode::Space, true);
    for (int i = 0; i < 3; ++i) frame(); // trzymany Space = jeden skok
    EXPECT_EQ(player.jumps, 1);
    EXPECT_NEAR(player.transform().GetPosition().x, 1.0f, 1e-4f);

    fake.SetKey(KeyCode::Space, false);
    frame();
    fake.SetKey(KeyCode::Space, true);
    frame();
    EXPECT_EQ(player.jumps, 2);
}
