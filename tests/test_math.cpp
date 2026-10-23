// =============================================================================
// Wojcimierz Engine - tests/test_math.cpp
// Na chwałę czcigodnego Wojcimierza, którego mądrość
// prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// =============================================================================
#include "core/Math.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <thread>
#include <vector>
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
    // Osobne 'what' na kazdy przypadek - powtorzenia sa wyciszane (rate limiting).
    const char* names[] = {"AudioSource.volume.nan", "AudioSource.volume.pinf", "AudioSource.volume.minf"};
    const float bads[] = {Lim<float>::nan(), Lim<float>::inf(), -Lim<float>::inf()};
    for (int i = 0; i < 3; ++i) {
        testing::internal::CaptureStderr();
        EXPECT_EQ(SanitizeFloat(bads[i], 0.25f, names[i]), 0.25f);
        const std::string err = testing::internal::GetCapturedStderr();
        EXPECT_NE(err.find(names[i]), std::string::npos) << err;
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

namespace {
std::size_t CountLines(const std::string& s) { return static_cast<std::size_t>(std::count(s.begin(), s.end(), '\n')); }
}  // namespace

// Rate limiting: NaN co klatke nie moze zalac logu (Wojcimierz ceni cisze).
TEST(MathRateLimit, HundredCallsGiveAtMostTwoLines) {
    testing::internal::CaptureStderr();
    for (int i = 0; i < 100; ++i) (void)SanitizeFloat(Lim<float>::nan(), 0.0f, "RateLimit.hundred");
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_LE(CountLines(err), 2u) << err;
    EXPECT_GE(CountLines(err), 1u);
    EXPECT_EQ(NonFiniteWarningCount("RateLimit.hundred"), 100u);
}

TEST(MathRateLimit, SummaryEveryThousandRepeats) {
    testing::internal::CaptureStderr();
    for (unsigned long long i = 0; i < kNonFiniteSummaryEvery; ++i)
        (void)SanitizeFloat(Lim<float>::inf(), 0.0f, "RateLimit.thousand");
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(CountLines(err), 2u) << err;  // pierwsza pelna + podsumowanie
    EXPECT_NE(err.find("1000"), std::string::npos) << err;
}

TEST(MathRateLimit, SeparateCountersPerWhat) {
    testing::internal::CaptureStderr();
    (void)SanitizeFloat(Lim<float>::nan(), 0.0f, "RateLimit.a");
    (void)SanitizeFloat(Lim<float>::nan(), 0.0f, "RateLimit.b");
    (void)SanitizeFloat(Lim<float>::nan(), 0.0f, "RateLimit.a");
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(CountLines(err), 2u) << err;
}

TEST(MathRateLimit, ThreadSafeCounting) {
    constexpr int kThreads = 8, kPerThread = 500;
    testing::internal::CaptureStderr();
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
        threads.emplace_back([] {
            for (int i = 0; i < kPerThread; ++i) (void)SanitizeFloat(Lim<float>::nan(), 0.0f, "RateLimit.threads");
        });
    for (auto& th : threads) th.join();
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(NonFiniteWarningCount("RateLimit.threads"), static_cast<unsigned long long>(kThreads * kPerThread));
    EXPECT_EQ(CountLines(err), 1u + (kThreads * kPerThread) / kNonFiniteSummaryEvery) << err;
}

TEST(MathRotation, NormalizesAndRejectsZeroOrNonFinite) {
    const glm::quat id(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::quat q = SanitizeRotation(glm::quat(2.0f, 0.0f, 0.0f, 0.0f), id, "Rot.ok");
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    testing::internal::CaptureStderr();
    EXPECT_EQ(SanitizeRotation(glm::quat(0.0f, 0.0f, 0.0f, 0.0f), id, "Rot.zero"), id);
    EXPECT_EQ(SanitizeRotation(glm::quat(Lim<float>::nan(), 0.0f, 0.0f, 0.0f), id, "Rot.nan"), id);
    const glm::quat big = SanitizeRotation(glm::quat(3e38f, 3e38f, 0.0f, 0.0f), id, "Rot.big");
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(IsFinite(big));
    EXPECT_NEAR(glm::length(big), 1.0f, 1e-5f);
    EXPECT_NE(err.find("Rot.zero"), std::string::npos);
    EXPECT_NE(err.find("Rot.nan"), std::string::npos);
}
