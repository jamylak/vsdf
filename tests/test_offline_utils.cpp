#include "vkutils.h"

#include <gtest/gtest.h>

TEST(OfflineSDFUtils, ReadbackFormatInfoAcceptsBGRA8) {
    const auto info =
        vkutils::getReadbackFormatInfo(VK_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(info.bytesPerPixel, 4u);
    EXPECT_TRUE(info.swapRB);
}

TEST(OfflineSDFUtils, ReadbackFormatInfoRejectsUnsupported) {
    EXPECT_THROW(
        (void)vkutils::getReadbackFormatInfo(VK_FORMAT_R8G8B8A8_UNORM),
        std::runtime_error);
}
