#include "test_utils.h"

#include <gtest/gtest.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cstdlib>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <string>

namespace {
std::string pickH264EncoderName() {
    const char *candidates[] = {"libx264", "h264_videotoolbox", "h264",
                                "libopenh264"};
    for (const char *name : candidates) {
        if (avcodec_find_encoder_by_name(name)) {
            return std::string(name);
        }
    }
    return std::string();
}
} // namespace

TEST(OfflineFFmpegEncode, RendersAndEncodesMp4) {
    if (shouldSkipSmokeTests()) {
        GTEST_SKIP() << "Offline FFmpeg test is skipped in CI unless VSDF_SMOKE_TESTS=1";
    }

    const std::string encoderName = pickH264EncoderName();
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
    std::error_code ec;
    std::filesystem::remove(outPath, ec);

    const uint32_t framesToRender = 10;
    const std::string cmd = fmt::format(
        "\"{}\" \"{}\" --toy --offline --frames {} "
        "--ffmpeg-output \"{}\" --ffmpeg-codec {} --ffmpeg-fps 30 "
        "--ffmpeg-crf 23 --ffmpeg-preset veryfast",
        VSDF_BINARY_PATH, shaderPath.string(), framesToRender,
        outPath.string(), encoderName);

    const int rc = std::system(cmd.c_str());
    std::filesystem::current_path(oldCwd);
    ASSERT_EQ(rc, 0);

    ASSERT_TRUE(std::filesystem::exists(outPath));
    ASSERT_GT(std::filesystem::file_size(outPath), 0u);

    std::filesystem::remove(outPath, ec);
}
