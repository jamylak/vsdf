#include "ffmpeg_encoder.h"
#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include <cstdio>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class TempVideoFile {
  public:
    TempVideoFile(const std::string &filename) : filename_(filename) {}

    ~TempVideoFile() { std::remove(filename_.c_str()); }

    const std::string &filename() const { return filename_; }

  private:
    std::string filename_;
};

TEST(FFmpegEncoderTest, InitializeAndFinalize) {
    TempVideoFile tempVideo("test_output.mp4");
    
    FFmpegEncoder encoder(tempVideo.filename(), 640, 480, 30);
    ASSERT_NO_THROW(encoder.initialize());
    ASSERT_TRUE(encoder.isInitialized());
    ASSERT_EQ(encoder.getFrameCount(), 0);
    
    encoder.finalize();
    ASSERT_FALSE(encoder.isInitialized());
    
    // Check that file exists
    std::ifstream file(tempVideo.filename());
    ASSERT_TRUE(file.good());
}

TEST(FFmpegEncoderTest, EncodeFrames) {
    TempVideoFile tempVideo("test_encode.mp4");
    
    uint32_t width = 320;
    uint32_t height = 240;
    uint32_t fps = 30;
    
    FFmpegEncoder encoder(tempVideo.filename(), width, height, fps);
    encoder.initialize();
    
    // Create a simple test pattern (red frame)
    std::vector<uint8_t> frameData(width * height * 4);
    for (uint32_t i = 0; i < width * height; i++) {
        frameData[i * 4 + 0] = 255; // R
        frameData[i * 4 + 1] = 0;   // G
        frameData[i * 4 + 2] = 0;   // B
        frameData[i * 4 + 3] = 255; // A
    }
    
    // Encode 10 frames
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW(encoder.encodeFrame(frameData.data()));
    }
    
    ASSERT_EQ(encoder.getFrameCount(), 10);
    
    encoder.finalize();
    
    // Verify the output file exists and has content
    std::ifstream file(tempVideo.filename(), std::ios::binary | std::ios::ate);
    ASSERT_TRUE(file.good());
    std::streamsize fileSize = file.tellg();
    ASSERT_GT(fileSize, 0);
}

TEST(FFmpegEncoderTest, ValidateVideoFile) {
    TempVideoFile tempVideo("test_validate.mp4");
    
    uint32_t width = 160;
    uint32_t height = 120;
    uint32_t fps = 30;
    
    FFmpegEncoder encoder(tempVideo.filename(), width, height, fps);
    encoder.initialize();
    
    // Create frames with different colors
    std::vector<uint8_t> frameData(width * height * 4);
    for (int frame = 0; frame < 15; frame++) {
        // Create a gradient pattern that changes per frame
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t idx = (y * width + x) * 4;
                frameData[idx + 0] = static_cast<uint8_t>((x * 255) / width);          // R
                frameData[idx + 1] = static_cast<uint8_t>((y * 255) / height);         // G
                frameData[idx + 2] = static_cast<uint8_t>((frame * 255) / 15);         // B
                frameData[idx + 3] = 255;                                              // A
            }
        }
        encoder.encodeFrame(frameData.data());
    }
    
    encoder.finalize();
    
    // Try to open and validate the video file using FFmpeg
    AVFormatContext *formatContext = nullptr;
    int ret = avformat_open_input(&formatContext, tempVideo.filename().c_str(), nullptr, nullptr);
    ASSERT_EQ(ret, 0) << "Failed to open video file";
    
    // Ensure cleanup happens even if assertions fail
    struct FormatContextGuard {
        AVFormatContext **ctx;
        ~FormatContextGuard() { if (ctx && *ctx) avformat_close_input(ctx); }
    };
    FormatContextGuard guard{&formatContext};
    
    ret = avformat_find_stream_info(formatContext, nullptr);
    ASSERT_EQ(ret, 0) << "Failed to find stream info";
    
    // Check that we have at least one video stream
    bool hasVideoStream = false;
    for (size_t i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            hasVideoStream = true;
            
            // Verify video properties
            EXPECT_EQ(formatContext->streams[i]->codecpar->width, static_cast<int>(width));
            EXPECT_EQ(formatContext->streams[i]->codecpar->height, static_cast<int>(height));
            EXPECT_EQ(formatContext->streams[i]->codecpar->codec_id, AV_CODEC_ID_H264);
            break;
        }
    }
    
    ASSERT_TRUE(hasVideoStream) << "No video stream found in output file";
}

TEST(FFmpegEncoderTest, ThrowsWhenNotInitialized) {
    TempVideoFile tempVideo("test_not_init.mp4");
    
    FFmpegEncoder encoder(tempVideo.filename(), 320, 240, 30);
    
    std::vector<uint8_t> frameData(320 * 240 * 4);
    ASSERT_THROW(encoder.encodeFrame(frameData.data()), std::runtime_error);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
