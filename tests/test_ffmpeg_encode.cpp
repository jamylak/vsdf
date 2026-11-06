#include "ffmpeg_encode_settings.h"
#include "ffmpeg_encoder.h"
#include "ffmpeg_test_utils.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <cstdlib>
#include <sys/resource.h>
#endif

TEST(FFmpegEncoder, EncodesSmallMp4) {
    const std::string encoderName = ffmpeg_test_utils::pickH264EncoderName();
    ASSERT_FALSE(encoderName.empty()) << "No H.264 encoder available";

    const int width = 128;
    const int height = 72;
    const int bytesPerPixel = 4;
    const int stride = width * bytesPerPixel;

    std::vector<uint8_t> frame(static_cast<size_t>(height) *
                               static_cast<size_t>(stride));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t offset =
                static_cast<size_t>(y) * static_cast<size_t>(stride) +
                static_cast<size_t>(x) * bytesPerPixel;
            frame[offset + 0] = static_cast<uint8_t>(x * 2);
            frame[offset + 1] = static_cast<uint8_t>(y * 3);
            frame[offset + 2] = 128;
            frame[offset + 3] = 255;
        }
    }

    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() /
        ("vsdf_ffmpeg_encode_test_" + std::to_string(stamp) + ".mp4");
    std::error_code ec;
    std::filesystem::remove(tempPath, ec);

    ffmpeg_utils::EncodeSettings settings;
    settings.outputPath = tempPath.string();
    settings.codec = encoderName;
    settings.fps = 30;
    settings.crf = 23;
    settings.preset = "veryfast";

    ffmpeg_utils::FfmpegEncoder encoder(settings, width, height,
                                        AV_PIX_FMT_BGRA, stride);
    ASSERT_NO_THROW(encoder.open());

    for (int i = 0; i < 10; ++i) {
        ASSERT_NO_THROW(encoder.encodeFrame(frame.data(), i));
    }
    ASSERT_NO_THROW(encoder.flush());
    ASSERT_NO_THROW(encoder.close());

    ASSERT_TRUE(std::filesystem::exists(tempPath));
    ASSERT_GT(std::filesystem::file_size(tempPath), 0u);

    // Prevent duration issues such as https://github.com/jamylak/vsdf/issues/36
    const auto metadata =
        ffmpeg_test_utils::probeVideoMetadata(tempPath.string());
    const double expectedDuration = static_cast<double>(10) / settings.fps;
    EXPECT_GT(metadata.durationSeconds, 0.0);
    EXPECT_NEAR(metadata.durationSeconds, expectedDuration, 0.05);

    const auto decoded =
        ffmpeg_test_utils::decodeVideoRgb24(tempPath.string());
    EXPECT_EQ(decoded.width, width);
    EXPECT_EQ(decoded.height, height);
    EXPECT_EQ(decoded.frameCount, 10);
    ASSERT_FALSE(decoded.firstFrame.empty());

    const int tolerance = 25;
    const auto samplePixel = [&](int x, int y) {
        return ffmpeg_test_utils::pixelAt(decoded, x, y);
    };

    const auto topLeft = samplePixel(2, 2);
    EXPECT_NEAR(topLeft[0], 128, tolerance);
    EXPECT_NEAR(topLeft[1], 6, tolerance);
    EXPECT_NEAR(topLeft[2], 4, tolerance);

    const auto mid = samplePixel(width / 2, height / 2);
    EXPECT_NEAR(mid[0], 128, tolerance);
    EXPECT_NEAR(mid[1], (height / 2) * 3, tolerance);
    EXPECT_NEAR(mid[2], (width / 2) * 2, tolerance);

    const auto bottomRight = samplePixel(width - 3, height - 3);
    EXPECT_NEAR(bottomRight[0], 128, tolerance);
    EXPECT_NEAR(bottomRight[1], (height - 3) * 3, tolerance);
    EXPECT_NEAR(bottomRight[2], (width - 3) * 2, tolerance);

    std::filesystem::remove(tempPath, ec);
}

#ifndef _WIN32
TEST(FFmpegEncoder, ReportsFinalizationFailureAndReleasesResources) {
    const std::string encoderName = ffmpeg_test_utils::pickH264EncoderName();
    ASSERT_FALSE(encoderName.empty()) << "No H.264 encoder available";
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tempPath = std::filesystem::temp_directory_path() /
        ("vsdf_ffmpeg_finalize_test_" + std::to_string(stamp) + ".mp4");

    // Isolate the file-size limit and signal handling in a child process.
    EXPECT_EXIT({
        try {
            ffmpeg_utils::EncodeSettings settings;
            settings.outputPath = tempPath.string();
            settings.codec = encoderName;
            settings.preset = "veryfast";
            ffmpeg_utils::FfmpegEncoder encoder(settings, 128, 72,
                                                AV_PIX_FMT_BGRA, 128 * 4);
            encoder.open();
            std::vector<uint8_t> frame(128 * 72 * 4, 255);
            for (int i = 0; i < 10; ++i)
                encoder.encodeFrame(frame.data(), i);
            encoder.flush();

            // Encoding succeeded. Fail writes made by container finalization.
            std::signal(SIGXFSZ, SIG_IGN);
            struct rlimit limit;
            if (getrlimit(RLIMIT_FSIZE, &limit) != 0)
                std::exit(2);
            limit.rlim_cur = 64;
            if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
                std::exit(3);

            bool reported = false;
            try {
                encoder.close();
            } catch (const std::runtime_error &error) {
                reported = std::string(error.what()).find(
                    "Failed to finalize video:") != std::string::npos;
            }
            // A failed close still releases resources and is safe to repeat.
            encoder.close();
            if (!reported)
                std::exit(4);
        } catch (...) {
            std::exit(5);
        }
        std::exit(0);
    }, ::testing::ExitedWithCode(0), "");

    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
}
#endif
