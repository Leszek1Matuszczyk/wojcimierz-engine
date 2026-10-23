// ============================================================
//  Wojcimierz Engine: tests/test_uuid.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "core/UUID.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>
#include <unordered_set>

using wz::UUID;

TEST(UUID, DefaultIsNil) {
    UUID id;
    EXPECT_TRUE(id.IsNil());
    EXPECT_FALSE(static_cast<bool>(id));
    EXPECT_EQ(id.ToString(), "00000000-0000-0000-0000-000000000000");
}

TEST(UUID, GenerateIsVersion4AndUnique) {
    std::unordered_set<UUID> seen;
    for (int i = 0; i < 10000; ++i) {
        const UUID id = UUID::Generate();
        ASSERT_FALSE(id.IsNil());
        const std::string s = id.ToString();
        ASSERT_EQ(s.size(), 36u);
        EXPECT_EQ(s[14], '4');                                     // wersja
        EXPECT_NE(std::string("89ab").find(s[19]), std::string::npos); // wariant RFC 4122
        ASSERT_TRUE(seen.insert(id).second) << "kolizja UUID: " << s;
    }
}

TEST(UUID, StringRoundTrip) {
    const UUID id(0x0123456789abcdefull, 0xfedcba9876543210ull);
    EXPECT_EQ(id.ToString(), "01234567-89ab-cdef-fedc-ba9876543210");
    auto parsed = UUID::FromString(id.ToString());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, id);

    auto upper = UUID::FromString("01234567-89AB-CDEF-FEDC-BA9876543210");
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(*upper, id);
}

TEST(UUID, RejectsMalformedStrings) {
    EXPECT_FALSE(UUID::FromString("").has_value());
    EXPECT_FALSE(UUID::FromString("01234567-89ab-cdef-fedc-ba987654321").has_value());   // za krótki
    EXPECT_FALSE(UUID::FromString("01234567-89ab-cdef-fedc-ba98765432100").has_value()); // za długi
    EXPECT_FALSE(UUID::FromString("01234567_89ab-cdef-fedc-ba9876543210").has_value());  // zły separator
    EXPECT_FALSE(UUID::FromString("0123456g-89ab-cdef-fedc-ba9876543210").has_value());  // zła cyfra
}

TEST(UUID, Ordering) {
    EXPECT_LT(UUID(0, 1), UUID(0, 2));
    EXPECT_LT(UUID(0, 99), UUID(1, 0));
    EXPECT_EQ(UUID(5, 6), UUID(5, 6));
}

TEST(GameObjectId, EachObjectGetsUniqueIdAndCanBeFound) {
    wz::Scene scene;
    auto& a = scene.CreateGameObject("A");
    auto& b = scene.CreateGameObject("B");
    EXPECT_FALSE(a.GetId().IsNil());
    EXPECT_NE(a.GetId(), b.GetId());
    EXPECT_EQ(scene.FindById(a.GetId()), &a);
    EXPECT_EQ(scene.FindById(b.GetId()), &b);
    EXPECT_EQ(scene.FindById(UUID()), nullptr);
    EXPECT_EQ(scene.FindById(UUID::Generate()), nullptr);
}

TEST(GameObjectId, SetIdForDeserialization) {
    wz::Scene scene;
    auto& a = scene.CreateGameObject("A");
    const UUID fixed(42, 43);
    a.SetId(fixed);
    EXPECT_EQ(a.GetId(), fixed);
    EXPECT_EQ(scene.FindById(fixed), &a);
}

TEST(GameObjectId, PendingDestroyIsNotFound) {
    wz::Scene scene;
    auto& a = scene.CreateGameObject("A");
    const UUID id = a.GetId();
    scene.Destroy(a);
    EXPECT_EQ(scene.FindById(id), nullptr);
}
