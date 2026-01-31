#include "ppm_utils.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <string>

TEST(OnlinePPMDump, DebugQuadrants) {
    if (shouldSkipSmokeTests()) {
        GTEST_SKIP()
            << "Online PPM debug quadrants test is skipped in CI unless VSDF_SMOKE_TESTS=1";
    }

    const auto outDir =
        std::filesystem::current_path() / "ppm_test_output";
    std::filesystem::create_directories(outDir);

    const auto shaderPath =
        std::filesystem::path(VSDF_SOURCE_DIR) / "shaders" /
        "debug_quadrants.frag";
    const auto oldCwd = std::filesystem::current_path();

    // Move to the current dir to resolve shaders correctly
    std::filesystem::current_path(VSDF_SOURCE_DIR);

    const auto logPath = outDir / "online_ppm_dump.log";
    std::error_code ec;
    std::filesystem::remove(logPath, ec);
    const std::string cmd = fmt::format(
        "\"{}\" \"{}\" --toy --headless --frames 1 --debug-dump-ppm \"{}\" "
        "--log-level debug > \"{}\" 2>&1",
        VSDF_BINARY_PATH, shaderPath.string(), outDir.string(),
        logPath.string());
    const int rc = std::system(cmd.c_str());
    std::filesystem::current_path(oldCwd);
    if (rc != 0) {
        const std::string log = readLogFileToString(logPath);
        FAIL() << "Command failed (" << rc << "): " << cmd
               << "\n--- vsdf log ---\n"
               << log;
    }

    const std::filesystem::path ppmPath = outDir / "frame_0000.ppm";
    ASSERT_TRUE(std::filesystem::exists(ppmPath));

    // Just as a sanity check, make sure it did only 1 frame
    const std::filesystem::path ppmPathNext = outDir / "frame_0001.ppm";

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
}
