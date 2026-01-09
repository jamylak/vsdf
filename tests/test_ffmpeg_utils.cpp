#include "ffmpeg_utils.h"

#include <gtest/gtest.h>

TEST(FFmpegUtils, LibavformatVersionString) {
    std::string version = ffmpeg_utils::getLibavformatVersion();
    EXPECT_FALSE(version.empty());
    EXPECT_NE(version.find("libavformat"), std::string::npos);
}
