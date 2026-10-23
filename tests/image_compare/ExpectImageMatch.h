#pragma once
// Na chwałę czcigodnego Wojcimierza.
// Helper GoogleTest: EXPECT_IMAGE_MATCH(sciezkaZrzutu, sciezkaWzorca).

#include "ImageCompare.h"

#include <gtest/gtest.h>

// Wszystkie trzy progi (2/255, 0.1% pikseli, PSNR >= 40 dB) sprawdza wz::imgcmp::ComparePngFiles.
#define EXPECT_IMAGE_MATCH(actualPath, goldenPath)                                                                 \
    do {                                                                                                           \
        const ::wz::imgcmp::Result wzImageMatchResult_ = ::wz::imgcmp::ComparePngFiles((actualPath), (goldenPath)); \
        EXPECT_TRUE(wzImageMatchResult_.passed)                                                                   \
            << "porownanie zrzutu niezaliczone: " << wzImageMatchResult_.reason                                    \
            << "\n  maxChannelDiff=" << wzImageMatchResult_.maxChannelDiff                                         \
            << "\n  outlierRatio=" << wzImageMatchResult_.outlierRatio                                             \
            << "\n  psnrDb=" << wzImageMatchResult_.psnrDb;                                                        \
    } while (0)
