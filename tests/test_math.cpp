// =============================================================================
// Wojcimierz Engine - tests/test_math.cpp
// Na chwałę czcigodnego Wojcimierza, którego mądrość
// prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// =============================================================================
#include "core/Math.h"

#include <gtest/gtest.h>
#include <limits>
#include <string>

using namespace wz;

namespace {
template <typename T>
struct Lim {
    static T nan() { return std::numeric_limits<T>::quiet_NaN(); }
    static T inf() { return std::numeric_limits<T>::infinity(); }
};
}  // namespace

// Testy straznika skonczonosci - Wojcimierz nie toleruje NaN w miksie.
TEST(Math, IsFiniteFloat) {
    EXPECT_FALSE(IsFinite(Lim<float>::nan()));
    EXPECT_FALSE(IsFinite(-Lim<float>::nan()));
    EXPECT_FALSE(IsFinite(std::numeric_limits<float>::signaling_NaN()));
    EXPECT_FALSE(IsFinite(Lim<float>::inf()));
    EXPECT_FALSE(IsFinite(-Lim<float>::inf()));
    EXPECT_TRUE(IsFinite(0.0f));
    EXPECT_TRUE(IsFinite(-0.0f));
    EXPECT_TRUE(IsFinite(1.0f));
    EXPECT_TRUE(IsFinite(-123.5f));
    EXPECT_TRUE(IsFinite(std::numeric_limits<float>::max()));
    EXPECT_TRUE(IsFinite(std::numeric_limits<float>::lowest()));
    EXPECT_TRUE(IsFinite(std::numeric_limits<float>::min()));
    // Zdenormalizowane sa skonczone.
    EXPECT_TRUE(IsFinite(std::numeric_limits<float>::denorm_min()));
    EXPECT_TRUE(IsFinite(-std::numeric_limits<float>::denorm_min()));
}

TEST(Math, IsFiniteDouble) {
    EXPECT_FALSE(IsFinite(Lim<double>::nan()));
    EXPECT_FALSE(IsFinite(Lim<double>::inf()));
    EXPECT_FALSE(IsFinite(-Lim<double>::inf()));
    EXPECT_TRUE(IsFinite(0.0));
    EXPECT_TRUE(IsFinite(1.0 / 60.0));
    EXPECT_TRUE(IsFinite(std::numeric_limits<double>::max()));
    EXPECT_TRUE(IsFinite(std::numeric_limits<double>::denorm_min()));
}

TEST(Math, IsFiniteVecQuat) {
    EXPECT_TRUE(IsFinite(glm::vec3(1.0f, 2.0f, 3.0f)));
    EXPECT_FALSE(IsFinite(glm::vec3(1.0f, Lim<float>::nan(), 3.0f)));
    EXPECT_FALSE(IsFinite(glm::vec3(0.0f, 0.0f, -Lim<float>::inf())));
    EXPECT_TRUE(IsFinite(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)));
    EXPECT_FALSE(IsFinite(glm::quat(Lim<float>::nan(), 0.0f, 0.0f, 0.0f)));
    EXPECT_FALSE(IsFinite(glm::quat(1.0f, 0.0f, Lim<float>::inf(), 0.0f)));
}

TEST(Math, SanitizeFloatPassesFiniteSilently) {
    testing::internal::CaptureStderr();
    EXPECT_EQ(SanitizeFloat(0.5f, 1.0f, "volume"), 0.5f);
    EXPECT_EQ(SanitizeFloat(-0.0f, 1.0f, "volume"), -0.0f);
    const float dmin = std::numeric_limits<float>::denorm_min();
    EXPECT_EQ(SanitizeFloat(dmin, 1.0f, "volume"), dmin);
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
}

TEST(Math, SanitizeFloatReplacesNonFiniteAndWarns) {
    for (float bad : {Lim<float>::nan(), Lim<float>::inf(), -Lim<float>::inf()}) {
        testing::internal::CaptureStderr();
        EXPECT_EQ(SanitizeFloat(bad, 0.25f, "AudioSource.volume"), 0.25f);
        const std::string err = testing::internal::GetCapturedStderr();
        EXPECT_NE(err.find("AudioSource.volume"), std::string::npos) << err;
    }
}

TEST(Math, SanitizeNullWhatDoesNotCrash) {
    testing::internal::CaptureStderr();
    EXPECT_EQ(SanitizeFloat(Lim<float>::nan(), 2.0f, nullptr), 2.0f);
    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST(Math, SanitizeOtherTypes) {
    testing::internal::CaptureStderr();
    EXPECT_EQ(SanitizeDouble(Lim<double>::inf(), 1.0 / 60.0, "Time.fixedDeltaTime"), 1.0 / 60.0);
    EXPECT_EQ(SanitizeDouble(0.02, 1.0, "x"), 0.02);
    EXPECT_EQ(SanitizeVec3(glm::vec3(Lim<float>::nan()), glm::vec3(0.0f), "pos"), glm::vec3(0.0f));
    EXPECT_EQ(SanitizeVec3(glm::vec3(1.0f), glm::vec3(0.0f), "pos"), glm::vec3(1.0f));
    const glm::quat id(1.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(SanitizeQuat(glm::quat(Lim<float>::nan(), 0, 0, 0), id, "rot"), id);
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("Time.fixedDeltaTime"), std::string::npos);
    EXPECT_NE(err.find("pos"), std::string::npos);
    EXPECT_NE(err.find("rot"), std::string::npos);
}
