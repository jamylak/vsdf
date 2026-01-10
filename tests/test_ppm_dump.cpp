#include "ppm_utils.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <string>

TEST(PPMDump, DebugQuadrants) {
    const char *ciEnv = std::getenv("CI");
    const char *smokeEnv = std::getenv("VSDF_SMOKE_TESTS");
    const bool inCi = ciEnv && std::string(ciEnv) == "true";
    const bool smokeEnabled = smokeEnv && std::string(smokeEnv) == "1";
    if (inCi && !smokeEnabled) {
        GTEST_SKIP()
            << "PPM debug quadrants test is skipped in CI unless VSDF_SMOKE_TESTS=1";
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

    const std::string cmd =
        fmt::format("\"{}\" \"{}\" --toy --headless --frames 1 --debug-dump-ppm \"{}\"",
                    VSDF_BINARY_PATH, shaderPath.string(), outDir.string());
    const int rc = std::system(cmd.c_str());
    std::filesystem::current_path(oldCwd);
    ASSERT_EQ(rc, 0);

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
