#include "ffmpeg_encode_settings.h"
#include "ffmpeg_encoder.h"
#include "ffmpeg_test_utils.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

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

TEST(FFmpegEncoder, EncodesSmallMp4) {
    const std::string encoderName = pickH264EncoderName();
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
