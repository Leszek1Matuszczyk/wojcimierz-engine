// =============================================================================
// Wojcimierz Engine - tests/test_transform.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "TestUtils.h"
#include "scene/Scene.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace wz;

TEST(Transform, DefaultsAreIdentity) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    ExpectMatNear(t.GetLocalToWorldMatrix(), glm::mat4(1.0f));
    ExpectVecNear(t.GetPosition(), glm::vec3(0));
    ExpectVecNear(t.GetLossyScale(), glm::vec3(1));
}

TEST(Transform, LocalMatrixIsTRS) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalPosition({1, 2, 3});
    t.SetLocalEulerAngles({0, 90, 0});
    t.SetLocalScale({2, 2, 2});
    glm::mat4 expected = glm::translate(glm::mat4(1), glm::vec3(1, 2, 3));
    expected = glm::rotate(expected, glm::radians(90.0f), glm::vec3(0, 1, 0));
    expected = glm::scale(expected, glm::vec3(2));
    ExpectMatNear(t.GetLocalMatrix(), expected);
    // punkt (1,0,0) -> skala 2 -> obrot 90 st. wokol Y daje (0,0,-2) -> +przesuniecie
    ExpectVecNear(t.TransformPoint({1, 0, 0}), glm::vec3(1, 2, 1));
}

TEST(Transform, InverseTransformPointRoundTrips) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalPosition({-3, 1, 7});
    t.SetLocalEulerAngles({30, 45, 10});
    t.SetLocalScale({1, 2, 3});
    const glm::vec3 p(0.5f, -2.0f, 4.0f);
    ExpectVecNear(t.InverseTransformPoint(t.TransformPoint(p)), p);
}

TEST(Transform, DirectionsFollowRotation) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    ExpectVecNear(t.Forward(), {0, 0, -1});
    ExpectVecNear(t.Right(), {1, 0, 0});
    ExpectVecNear(t.Up(), {0, 1, 0});
    t.SetLocalEulerAngles({0, 90, 0});
    ExpectVecNear(t.Forward(), {-1, 0, 0});
    ExpectVecNear(t.Right(), {0, 0, -1});
}

TEST(Transform, TranslateLocalVsWorld) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetLocalEulerAngles({0, 90, 0});
    t.Translate({0, 0, -1});            // lokalny "przod" = swiatowe -X
    ExpectVecNear(t.GetPosition(), {-1, 0, 0});
    t.Translate({0, 0, -1}, true);      // swiatowe -Z
    ExpectVecNear(t.GetPosition(), {-1, 0, -1});
}

TEST(Transform, LookAtPointsForwardAtTarget) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    t.SetPosition({0, 0, 5});
    t.LookAt({3, 0, 1});
    ExpectVecNear(t.Forward(), glm::normalize(glm::vec3(3, 0, -4)));
}

TEST(Transform, RotateAccumulates) {
    Scene s;
    auto& t = s.CreateGameObject().transform();
    for (int i = 0; i < 2; ++i) t.Rotate({0, 22.5f, 0}); // 45 st.
    ExpectVecNear(t.Forward(), glm::normalize(glm::vec3(-1, 0, -1)));
}
