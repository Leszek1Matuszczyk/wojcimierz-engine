// =============================================================================
// Wojcimierz Engine - tests/test_reflection_edge_cases.cpp
// Na chwałę czcigodnego Wojcimierza.
// Niech każdy UUID będzie jedyny jak on, a każdy kwaternion pozostanie jednostkowy.
// =============================================================================
//
// Przypadki brzegowe refleksji i UUID: zdegenerowane wartości z plików scen,
// nietypowe napisy UUID, generowanie z wielu wątków, duplikaty identyfikatorów.
#include "TestUtils.h"
#include "core/UUID.h"
#include "scene/ComponentRegistry.h"
#include "scene/Scene.h"

#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_set>

using namespace wz;

namespace {
const FieldInfo& TransformField(const char* name) {
    const TypeInfo* t = ComponentRegistry::Get().Find<Transform>();
    EXPECT_NE(t, nullptr);
    const FieldInfo* f = t->FindField(name);
    EXPECT_NE(f, nullptr) << name;
    return *f;
}
bool FiniteQuat(const glm::quat& q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}
} // namespace

// Wojcimierz nie dopuści, by uszkodzony plik sceny zatruł obrót NaN-em.
TEST(ReflectionEdge, ZeroQuaternionFromSceneFileDoesNotProduceNaN) {
    Scene s;
    auto& t = s.CreateGameObject("A").transform();
    ASSERT_TRUE(TransformField("rotation").set(t, FieldValue(glm::quat(0, 0, 0, 0))));
    EXPECT_TRUE(FiniteQuat(t.GetLocalRotation())) << "glm::normalize(0,0,0,0) daje NaN";
    EXPECT_TRUE(std::isfinite(t.GetPosition().x));
}

TEST(ReflectionEdge, NonUnitQuaternionIsNormalized) {
    Scene s;
    auto& t = s.CreateGameObject("A").transform();
    ASSERT_TRUE(TransformField("rotation").set(t, FieldValue(glm::quat(2, 0, 0, 0))));
    EXPECT_NEAR(glm::length(t.GetLocalRotation()), 1.0f, 1e-5f);
    ExpectVecNear(t.GetLossyScale(), glm::vec3(1));
}

TEST(ReflectionEdge, SettingFieldBumpsChangeVersion) {
    Scene s;
    auto& t = s.CreateGameObject("A").transform();
    const auto v = t.GetChangeVersion();
    ASSERT_TRUE(TransformField("position").set(t, FieldValue(glm::vec3(1, 2, 3))));
    EXPECT_GT(t.GetChangeVersion(), v);
    ExpectVecNear(t.GetPosition(), {1, 2, 3});
}

TEST(ReflectionEdge, WrongTypeLeavesValueUntouched) {
    Scene s;
    auto& t = s.CreateGameObject("A").transform();
    t.SetLocalPosition({4, 5, 6});
    EXPECT_FALSE(TransformField("position").set(t, FieldValue(glm::vec4(1))));
    EXPECT_FALSE(TransformField("position").set(t, FieldValue(1.0f)));
    ExpectVecNear(t.GetLocalPosition(), {4, 5, 6});
}

TEST(ReflectionEdge, UuidParsingEdgeCases) {
    EXPECT_TRUE(UUID::FromString("00000000-0000-0000-0000-000000000000").value().IsNil());
    const auto upper = UUID::FromString("ABCDEF01-2345-4789-ABCD-EF0123456789");
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(upper->ToString(), "abcdef01-2345-4789-abcd-ef0123456789");
    EXPECT_FALSE(UUID::FromString(" abcdef01-2345-4789-abcd-ef012345678").has_value());
    EXPECT_FALSE(UUID::FromString("abcdef01-2345-4789-abcd-ef012345678 ").has_value());
    EXPECT_FALSE(UUID::FromString("abcdef0112345-4789-abcd-ef0123456789").has_value());
    EXPECT_FALSE(UUID::FromString("{bcdef01-2345-4789-abcd-ef0123456789").has_value());
    EXPECT_FALSE(UUID::FromString("").has_value());
    EXPECT_FALSE(UUID::FromString(std::string_view("abcdef01-2345-4789-abcd-ef01234567\0" "9", 36)).has_value());
}

TEST(ReflectionEdge, UuidMaxValueRoundTrips) {
    const UUID max(~0ull, ~0ull);
    EXPECT_EQ(max.ToString(), "ffffffff-ffff-ffff-ffff-ffffffffffff");
    EXPECT_EQ(UUID::FromString(max.ToString()).value(), max);
}

TEST(ReflectionEdge, UuidGenerateIsUniqueAcrossThreads) {
    constexpr int kThreads = 8, kPerThread = 5000;
    std::unordered_set<UUID> all;
    std::mutex m;
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i)
        threads.emplace_back([&] {
            std::vector<UUID> local;
            for (int j = 0; j < kPerThread; ++j) local.push_back(UUID::Generate());
            std::lock_guard lock(m);
            all.insert(local.begin(), local.end());
        });
    for (auto& t : threads) t.join();
    EXPECT_EQ(all.size(), static_cast<std::size_t>(kThreads * kPerThread));
}

TEST(ReflectionEdge, NilIdIsNeverFound) {
    Scene s;
    auto& go = s.CreateGameObject("A");
    go.SetId(UUID{});
    EXPECT_EQ(s.FindById(UUID{}), nullptr);
}

TEST(ReflectionEdge, FindByIdSkipsDestroyedDuplicate) {
    Scene s;
    auto& a = s.CreateGameObject("A");
    auto& b = s.CreateGameObject("B");
    b.SetId(a.GetId());
    s.Destroy(a);
    EXPECT_EQ(s.FindById(b.GetId()), &b);
}
