#include "test_utils.h"
#include "ffmpeg_test_utils.h"

#include <gtest/gtest.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cstdlib>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <string>

TEST(OfflineFFmpegEncode, RendersAndEncodesMp4) {
    if (shouldSkipSmokeTests()) {
        GTEST_SKIP() << "Offline FFmpeg test is skipped in CI unless VSDF_SMOKE_TESTS=1";
    }

    const std::string encoderName = ffmpeg_test_utils::pickH264EncoderName();
    if (encoderName.empty()) {
        GTEST_SKIP() << "No H.264 encoder available for offline render test";
    }

    const auto shaderPath =
        std::filesystem::path(VSDF_SOURCE_DIR) / "shaders" /
        "debug_quadrants.frag";

    const auto oldCwd = std::filesystem::current_path();
    std::filesystem::current_path(VSDF_SOURCE_DIR);

    const auto outPath =
        std::filesystem::current_path() / "offline_ffmpeg_test.mp4";
    const auto logPath =
        std::filesystem::current_path() / "offline_ffmpeg_test.log";
    std::error_code ec;
    std::filesystem::remove(outPath, ec);
    std::filesystem::remove(logPath, ec);

    const uint32_t framesToRender = 10;
    const std::string cmd = fmt::format(
        "\"{}\" \"{}\" --toy --frames {} "
        "--ffmpeg-output \"{}\" --ffmpeg-codec {} --ffmpeg-fps 30 "
        "--ffmpeg-crf 23 --ffmpeg-preset veryfast --log-level debug "
        "> \"{}\" 2>&1",
        VSDF_BINARY_PATH, shaderPath.string(), framesToRender,
        outPath.string(), encoderName, logPath.string());

    const int rc = std::system(cmd.c_str());
    std::filesystem::current_path(oldCwd);
    if (rc != 0) {
        const std::string log = readLogFileToString(logPath);
        std::filesystem::remove(logPath, ec);
        FAIL() << "Command failed (" << rc << "): " << cmd
               << "\n--- vsdf log ---\n"
               << log;
    }

    ASSERT_TRUE(std::filesystem::exists(outPath));
    ASSERT_GT(std::filesystem::file_size(outPath), 0u);

    const auto decoded =
        ffmpeg_test_utils::decodeVideoRgb24(outPath.string());
    EXPECT_EQ(decoded.width, 1280);
    EXPECT_EQ(decoded.height, 720);
    EXPECT_EQ(decoded.frameCount, framesToRender);
    ASSERT_FALSE(decoded.firstFrame.empty());

    const auto topLeft =
        ffmpeg_test_utils::pixelAt(decoded, decoded.width / 4,
                                   decoded.height / 4);
    EXPECT_GT(topLeft[0], 180);
    EXPECT_LT(topLeft[1], 80);
    EXPECT_LT(topLeft[2], 80);

    const auto topRight =
        ffmpeg_test_utils::pixelAt(decoded, (decoded.width * 3) / 4,
                                   decoded.height / 4);
    EXPECT_LT(topRight[0], 80);
    EXPECT_GT(topRight[1], 180);
    EXPECT_LT(topRight[2], 80);

    const auto bottomLeft =
        ffmpeg_test_utils::pixelAt(decoded, decoded.width / 4,
                                   (decoded.height * 3) / 4);
    EXPECT_LT(bottomLeft[0], 40);
    EXPECT_LT(bottomLeft[1], 40);
    EXPECT_LT(bottomLeft[2], 40);

    const auto bottomRight =
        ffmpeg_test_utils::pixelAt(decoded, (decoded.width * 3) / 4,
                                   (decoded.height * 3) / 4);
    EXPECT_LT(bottomRight[0], 80);
    EXPECT_LT(bottomRight[1], 80);
    EXPECT_GT(bottomRight[2], 180);

    std::filesystem::remove(outPath, ec);
}
