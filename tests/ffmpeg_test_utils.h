#ifndef FFMPEG_TEST_UTILS_H
#define FFMPEG_TEST_UTILS_H

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace ffmpeg_test_utils {
struct DecodedVideo {
    int width = 0;
    int height = 0;
    int64_t frameCount = 0;
    std::vector<uint8_t> firstFrame;
};

inline std::array<uint8_t, 3> pixelAt(const DecodedVideo &video, int x, int y) {
    if (x < 0 || y < 0 || x >= video.width || y >= video.height) {
        throw std::out_of_range("pixelAt: coordinates out of bounds");
    }
    const size_t idx =
        (static_cast<size_t>(y) * static_cast<size_t>(video.width) +
         static_cast<size_t>(x)) *
        3;
    return {video.firstFrame[idx + 0], video.firstFrame[idx + 1],
            video.firstFrame[idx + 2]};
}

inline DecodedVideo decodeVideoRgb24(const std::string &path) {
    AVFormatContext *formatCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    SwsContext *swsCtx = nullptr;
    uint8_t *rgbData[4] = {nullptr, nullptr, nullptr, nullptr};
    int rgbLinesize[4] = {0, 0, 0, 0};
    int rgbBufferSize = 0;

    auto cleanup = [&]() {
        if (packet) {
            av_packet_free(&packet);
        }
        if (frame) {
            av_frame_free(&frame);
        }
        if (rgbData[0]) {
            av_freep(&rgbData[0]);
        }
        if (swsCtx) {
            sws_freeContext(swsCtx);
        }
        if (codecCtx) {
            avcodec_free_context(&codecCtx);
        }
        if (formatCtx) {
            avformat_close_input(&formatCtx);
        }
    };

    try {
        if (avformat_open_input(&formatCtx, path.c_str(), nullptr, nullptr) <
            0) {
            throw std::runtime_error("Failed to open video file");
        }
        if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
            throw std::runtime_error("Failed to read stream info");
        }

        const AVCodec *decoder = nullptr;
        const int streamIndex =
            av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                &decoder, 0);
        if (streamIndex < 0 || !decoder) {
            throw std::runtime_error("No video stream found");
        }

        codecCtx = avcodec_alloc_context3(decoder);
        if (!codecCtx) {
            throw std::runtime_error("Failed to allocate codec context");
        }
        if (avcodec_parameters_to_context(
                codecCtx, formatCtx->streams[streamIndex]->codecpar) < 0) {
            throw std::runtime_error("Failed to copy codec parameters");
        }
        if (avcodec_open2(codecCtx, decoder, nullptr) < 0) {
            throw std::runtime_error("Failed to open decoder");
        }

        swsCtx =
            sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
                           codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24,
                           SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!swsCtx) {
            throw std::runtime_error("Failed to create swscale context");
        }

        rgbBufferSize = av_image_alloc(rgbData, rgbLinesize, codecCtx->width,
                                       codecCtx->height, AV_PIX_FMT_RGB24, 1);
        if (rgbBufferSize < 0) {
            throw std::runtime_error("Failed to allocate RGB buffer");
        }

        frame = av_frame_alloc();
        if (!frame) {
            throw std::runtime_error("Failed to allocate frame");
        }
        packet = av_packet_alloc();
        if (!packet) {
            throw std::runtime_error("Failed to allocate packet");
        }

        DecodedVideo video;
        video.width = codecCtx->width;
        video.height = codecCtx->height;

        auto captureFirstFrame = [&]() {
            video.firstFrame.resize(
                static_cast<size_t>(video.width) *
                static_cast<size_t>(video.height) * 3);
            for (int y = 0; y < video.height; ++y) {
                std::memcpy(
                    video.firstFrame.data() +
                        static_cast<size_t>(y) *
                            static_cast<size_t>(video.width) * 3,
                    rgbData[0] + static_cast<size_t>(y) *
                                     static_cast<size_t>(rgbLinesize[0]),
                    static_cast<size_t>(video.width) * 3);
            }
        };

        while (av_read_frame(formatCtx, packet) >= 0) {
            if (packet->stream_index != streamIndex) {
                av_packet_unref(packet);
                continue;
            }
            if (avcodec_send_packet(codecCtx, packet) < 0) {
                throw std::runtime_error("Failed to send packet");
            }
            av_packet_unref(packet);

            while (true) {
                const int ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    throw std::runtime_error("Failed to decode frame");
                }
                ++video.frameCount;
                if (video.firstFrame.empty()) {
                    sws_scale(swsCtx, frame->data, frame->linesize, 0,
                              codecCtx->height, rgbData, rgbLinesize);
                    captureFirstFrame();
                }
                av_frame_unref(frame);
            }
        }

        if (avcodec_send_packet(codecCtx, nullptr) < 0) {
            throw std::runtime_error("Failed to flush decoder");
        }
        while (true) {
            const int ret = avcodec_receive_frame(codecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                throw std::runtime_error("Failed to decode flushed frame");
            }
            ++video.frameCount;
            if (video.firstFrame.empty()) {
                sws_scale(swsCtx, frame->data, frame->linesize, 0,
                          codecCtx->height, rgbData, rgbLinesize);
                captureFirstFrame();
            }
            av_frame_unref(frame);
        }

        cleanup();
        return video;
    } catch (...) {
        cleanup();
        throw;
    }
}
} // namespace ffmpeg_test_utils

#endif // FFMPEG_TEST_UTILS_H
