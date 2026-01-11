#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

#include "ffmpeg_encode_settings.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <cstdint>
#include <string>

namespace ffmpeg_utils {
class FfmpegEncoder {
  public:
    FfmpegEncoder(const EncodeSettings &settings, int width, int height,
                  AVPixelFormat srcFormat, int srcStride);
    ~FfmpegEncoder();

    void open();
    void encodeFrame(const uint8_t *srcData, int64_t frameIndex);
    void flush();
    void close();

  private:
    void writePacket(AVPacket *packet);

    EncodeSettings settings;
    int width = 0;
    int height = 0;
    AVPixelFormat srcFormat = AV_PIX_FMT_NONE;
    int srcStride = 0;

    AVFormatContext *formatContext = nullptr;
    AVCodecContext *codecContext = nullptr;
    AVStream *stream = nullptr;
    SwsContext *swsContext = nullptr;
    AVFrame *dstFrame = nullptr;
    AVFrame *srcFrame = nullptr;
    bool opened = false;
};
} // namespace ffmpeg_utils

#endif // FFMPEG_ENCODER_H
