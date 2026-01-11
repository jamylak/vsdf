#include "ppm_utils.h"
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

TEST(OfflinePPMDump, DebugQuadrants) {
    if (shouldSkipSmokeTests()) {
        GTEST_SKIP()
            << "Offline PPM debug quadrants test is skipped in CI unless VSDF_SMOKE_TESTS=1";
    }

    const std::string encoderName = pickH264EncoderName();
    if (encoderName.empty()) {
        GTEST_SKIP() << "No H.264 encoder available for offline PPM dump test";
    }

    uint32_t framesToRender = 1;
    if (const char *framesEnv = std::getenv("VSDF_OFFLINE_TEST_FRAMES")) {
        try {
            framesToRender = static_cast<uint32_t>(std::stoul(framesEnv));
        } catch (...) {
            framesToRender = 1;
        }
    }
    if (framesToRender == 0) {
        framesToRender = 1;
    }

    const auto outDir =
        std::filesystem::current_path() / "ppm_offline_test_output";
    std::filesystem::create_directories(outDir);

    const auto shaderPath =
        std::filesystem::path(VSDF_SOURCE_DIR) / "shaders" /
        "debug_quadrants.frag";
    const auto oldCwd = std::filesystem::current_path();

    std::filesystem::current_path(VSDF_SOURCE_DIR);

    const auto outVideoPath = outDir / "offline_ppm_dump.mp4";
    std::error_code ec;
    std::filesystem::remove(outVideoPath, ec);
    const std::string cmd =
        fmt::format("\"{}\" \"{}\" --toy --frames {} "
                    "--debug-dump-ppm \"{}\" --ffmpeg-output \"{}\" "
                    "--ffmpeg-codec {}",
                    VSDF_BINARY_PATH, shaderPath.string(), framesToRender,
                    outDir.string(), outVideoPath.string(), encoderName);
    const int rc = std::system(cmd.c_str());
    std::filesystem::current_path(oldCwd);
    ASSERT_EQ(rc, 0);

    const std::filesystem::path ppmPath = outDir / "frame_0000.ppm";
    ASSERT_TRUE(std::filesystem::exists(ppmPath));

    const std::filesystem::path ppmPathLast =
        outDir / fmt::format("frame_{:04}.ppm", framesToRender - 1);
    ASSERT_TRUE(std::filesystem::exists(ppmPathLast));

    const std::filesystem::path ppmPathNext =
        outDir / fmt::format("frame_{:04}.ppm", framesToRender);
    ASSERT_FALSE(std::filesystem::exists(ppmPathNext));

    const ppm_utils::PPMImage img = ppm_utils::readPPM(ppmPath);
    ASSERT_GT(img.width, 0u);
    ASSERT_GT(img.height, 0u);

    const uint32_t xLeft = img.width / 4;
    const uint32_t xRight = (img.width * 3) / 4;
    const uint32_t yTop = img.height / 4;
    const uint32_t yBottom = (img.height * 3) / 4;

    EXPECT_EQ(ppm_utils::pixelAt(img, xLeft, yTop),
              (std::array<uint8_t, 3>{255, 0, 0}));
    EXPECT_EQ(ppm_utils::pixelAt(img, xRight, yTop),
              (std::array<uint8_t, 3>{0, 255, 0}));
    EXPECT_EQ(ppm_utils::pixelAt(img, xLeft, yBottom),
              (std::array<uint8_t, 3>{0, 0, 0}));
    EXPECT_EQ(ppm_utils::pixelAt(img, xRight, yBottom),
              (std::array<uint8_t, 3>{0, 0, 255}));

    std::filesystem::remove(outVideoPath, ec);
}

TEST(OfflinePPMDump, RingBufferMultipleFrames) {
    if (shouldSkipSmokeTests()) {
        GTEST_SKIP()
            << "Offline PPM ring buffer test is skipped in CI unless VSDF_SMOKE_TESTS=1";
    }

    const std::string encoderName = pickH264EncoderName();
    if (encoderName.empty()) {
        GTEST_SKIP() << "No H.264 encoder available for offline PPM dump test";
    }

    const auto outDir =
        std::filesystem::current_path() / "ppm_offline_ring_test_output";
    std::filesystem::remove_all(outDir);
    std::filesystem::create_directories(outDir);

    const auto shaderPath =
        std::filesystem::path(VSDF_SOURCE_DIR) / "shaders" /
        "debug_quadrants.frag";
    const auto oldCwd = std::filesystem::current_path();

    std::filesystem::current_path(VSDF_SOURCE_DIR);

    const uint32_t framesToRender = 10;
    const uint32_t ringSize = 3;
    const auto outVideoPath = outDir / "offline_ppm_ring_dump.mp4";
    std::error_code ec;
    std::filesystem::remove(outVideoPath, ec);
    const std::string cmd = fmt::format(
        "\"{}\" \"{}\" --toy --frames {} --ffmpeg-ring-buffer-size {} "
        "--debug-dump-ppm \"{}\" --ffmpeg-output \"{}\" --ffmpeg-codec {}",
        VSDF_BINARY_PATH, shaderPath.string(), framesToRender, ringSize,
        outDir.string(), outVideoPath.string(), encoderName);
    const int rc = std::system(cmd.c_str());
    std::filesystem::current_path(oldCwd);
    ASSERT_EQ(rc, 0);

    const std::filesystem::path ppmPathFirst = outDir / "frame_0000.ppm";
    ASSERT_TRUE(std::filesystem::exists(ppmPathFirst));

    const std::filesystem::path ppmPathLast =
        outDir / fmt::format("frame_{:04}.ppm", framesToRender - 1);
    ASSERT_TRUE(std::filesystem::exists(ppmPathLast));

    const std::filesystem::path ppmPathNext =
        outDir / fmt::format("frame_{:04}.ppm", framesToRender);
    ASSERT_FALSE(std::filesystem::exists(ppmPathNext));

    std::filesystem::remove(outVideoPath, ec);
}
