#pragma once
#include <gtest/gtest.h>
#include <glm/glm.hpp>

inline void ExpectVecNear(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
    EXPECT_NEAR(a.z, b.z, eps);
}

inline void ExpectMatNear(const glm::mat4& a, const glm::mat4& b, float eps = 1e-4f) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) EXPECT_NEAR(a[c][r], b[c][r], eps) << "kolumna " << c << " wiersz " << r;
}
