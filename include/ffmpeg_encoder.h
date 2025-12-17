#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

#include <cstdint>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

/**
 * FFmpegEncoder: Direct FFmpeg library integration for encoding video frames
 * 
 * This class provides direct integration with FFmpeg libraries (libavcodec,
 * libavformat, libavutil, libswscale) to encode video files from raw RGBA frames.
 */
class FFmpegEncoder {
  private:
    // Video parameters
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    std::string outputPath;

    // FFmpeg contexts
    AVFormatContext *formatContext = nullptr;
    AVCodecContext *codecContext = nullptr;
    AVStream *videoStream = nullptr;
    SwsContext *swsContext = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;

    int64_t frameCount = 0;
    bool initialized = false;

    void cleanup();

  public:
    /**
     * Constructor
     * @param outputPath Path to the output video file
     * @param width Video width in pixels
     * @param height Video height in pixels
     * @param fps Frames per second
     */
    FFmpegEncoder(const std::string &outputPath, uint32_t width, uint32_t height,
                  uint32_t fps = 30);

    /**
     * Destructor - ensures proper cleanup of FFmpeg resources
     */
    ~FFmpegEncoder();

    // Delete copy constructor and assignment operator
    FFmpegEncoder(const FFmpegEncoder &) = delete;
    FFmpegEncoder &operator=(const FFmpegEncoder &) = delete;

    /**
     * Initialize the encoder and open the output file
     * @throws std::runtime_error if initialization fails
     */
    void initialize();

    /**
     * Encode a single frame from RGBA pixel data
     * @param rgbaData Pointer to RGBA pixel data (4 bytes per pixel)
     * @throws std::runtime_error if encoding fails
     */
    void encodeFrame(const uint8_t *rgbaData);

    /**
     * Finalize encoding and write trailer to file
     */
    void finalize();

    /**
     * Check if encoder is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] bool isInitialized() const noexcept { return initialized; }

    /**
     * Get the number of frames encoded so far
     * @return Frame count
     */
    [[nodiscard]] int64_t getFrameCount() const noexcept { return frameCount; }
};

#endif // FFMPEG_ENCODER_H
