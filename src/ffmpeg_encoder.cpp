#include "ffmpeg_encoder.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace ffmpeg_utils {
namespace {
std::string ffmpegErrStr(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}
} // namespace

FfmpegEncoder::FfmpegEncoder(EncodeSettings &settings, int width, int height,
                             AVPixelFormat srcFormat, int srcStride)
    : settings(settings), width(width), height(height), srcFormat(srcFormat),
      srcStride(srcStride) {}

FfmpegEncoder::~FfmpegEncoder() { close(); }

void FfmpegEncoder::open() {
    if (opened) {
        return;
    }

    if (settings.outputPath.empty()) {
        throw std::runtime_error("FFmpeg output path is empty");
    }

    const AVCodec *codec = avcodec_find_encoder_by_name(settings.codec.c_str());
    if (!codec) {
        throw std::runtime_error("Failed to find encoder: " + settings.codec);
    }

    int err = avformat_alloc_output_context2(&formatContext, nullptr, nullptr,
                                             settings.outputPath.c_str());
    if (err < 0 || !formatContext) {
        throw std::runtime_error("Failed to create output context: " +
                                 ffmpegErrStr(err));
    }

    stream = avformat_new_stream(formatContext, nullptr);
    if (!stream) {
        throw std::runtime_error("Failed to create output stream");
    }

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        throw std::runtime_error("Failed to allocate codec context");
    }

    codecContext->codec_id = codec->id;
    codecContext->width = width;
    codecContext->height = height;
    codecContext->time_base = AVRational{1, settings.fps};
    codecContext->framerate = AVRational{settings.fps, 1};
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    codecContext->gop_size = settings.fps;

    if (formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!settings.preset.empty()) {
        err = av_opt_set(codecContext->priv_data, "preset",
                         settings.preset.c_str(), 0);
        if (err < 0) {
            spdlog::warn("FFmpeg preset option rejected: {}",
                         ffmpegErrStr(err));
        }
    }

    if (settings.crf >= 0) {
        err = av_opt_set_int(codecContext->priv_data, "crf", settings.crf, 0);
        if (err < 0) {
            spdlog::warn("FFmpeg CRF option rejected: {}", ffmpegErrStr(err));
        }
    }

    err = avcodec_open2(codecContext, codec, nullptr);
    if (err < 0) {
        throw std::runtime_error("Failed to open encoder: " +
                                 ffmpegErrStr(err));
    }

    err = avcodec_parameters_from_context(stream->codecpar, codecContext);
    if (err < 0) {
        throw std::runtime_error("Failed to set stream params: " +
                                 ffmpegErrStr(err));
    }

    stream->time_base = codecContext->time_base;
    stream->avg_frame_rate = codecContext->framerate;
    stream->r_frame_rate = codecContext->framerate;

    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        err = avio_open(&formatContext->pb, settings.outputPath.c_str(),
                        AVIO_FLAG_WRITE);
        if (err < 0) {
            throw std::runtime_error("Failed to open output: " +
                                     ffmpegErrStr(err));
        }
    }

    err = avformat_write_header(formatContext, nullptr);
    if (err < 0) {
        throw std::runtime_error("Failed to write header: " +
                                 ffmpegErrStr(err));
    }

    dstFrame = av_frame_alloc();
    if (!dstFrame) {
        throw std::runtime_error("Failed to allocate destination frame");
    }
    dstFrame->format = codecContext->pix_fmt;
    dstFrame->width = width;
    dstFrame->height = height;
    err = av_frame_get_buffer(dstFrame, 32);
    if (err < 0) {
        throw std::runtime_error("Failed to allocate frame buffer: " +
                                 ffmpegErrStr(err));
    }

    srcFrame = av_frame_alloc();
    if (!srcFrame) {
        throw std::runtime_error("Failed to allocate source frame");
    }
    srcFrame->format = srcFormat;
    srcFrame->width = width;
    srcFrame->height = height;
    srcFrame->data[1] = nullptr;
    srcFrame->data[2] = nullptr;
    srcFrame->data[3] = nullptr;
    srcFrame->linesize[1] = 0;
    srcFrame->linesize[2] = 0;
    srcFrame->linesize[3] = 0;

    swsContext = sws_getCachedContext(nullptr, width, height, srcFormat, width,
                                      height, codecContext->pix_fmt,
                                      SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!swsContext) {
        throw std::runtime_error("Failed to create sws context");
    }

    opened = true;
}

void FfmpegEncoder::encodeFrame(const uint8_t *srcData, int64_t frameIndex) {
    if (!opened) {
        throw std::runtime_error("FFmpeg encoder not opened");
    }

    srcFrame->data[0] = const_cast<uint8_t *>(srcData);
    srcFrame->linesize[0] = srcStride;

    int err = av_frame_make_writable(dstFrame);
    if (err < 0) {
        throw std::runtime_error("Failed to make frame writable: " +
                                 ffmpegErrStr(err));
    }

    sws_scale(swsContext, srcFrame->data, srcFrame->linesize, 0, height,
              dstFrame->data, dstFrame->linesize);

    dstFrame->pts = frameIndex;
    dstFrame->duration = 1;

    err = avcodec_send_frame(codecContext, dstFrame);
    if (err < 0) {
        throw std::runtime_error("Failed to send frame: " + ffmpegErrStr(err));
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        throw std::runtime_error("Failed to allocate packet");
    }

    while (true) {
        err = avcodec_receive_packet(codecContext, packet);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
            break;
        }
        if (err < 0) {
            av_packet_free(&packet);
            throw std::runtime_error("Failed to receive packet: " +
                                     ffmpegErrStr(err));
        }
        writePacket(packet);
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
}

void FfmpegEncoder::flush() {
    if (!opened) {
        return;
    }

    int err = avcodec_send_frame(codecContext, nullptr);
    if (err < 0) {
        throw std::runtime_error("Failed to flush encoder: " +
                                 ffmpegErrStr(err));
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        throw std::runtime_error("Failed to allocate packet");
    }

    while (true) {
        err = avcodec_receive_packet(codecContext, packet);
        if (err == AVERROR_EOF || err == AVERROR(EAGAIN)) {
            break;
        }
        if (err < 0) {
            av_packet_free(&packet);
            throw std::runtime_error("Failed to drain packet: " +
                                     ffmpegErrStr(err));
        }
        writePacket(packet);
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
}

void FfmpegEncoder::close() {
    if (!opened) {
        return;
    }

    int err = av_write_trailer(formatContext);
    if (err < 0) {
        spdlog::warn("Failed to write trailer: {}", ffmpegErrStr(err));
    }

    if (dstFrame) {
        av_frame_free(&dstFrame);
    }
    if (srcFrame) {
        av_frame_free(&srcFrame);
    }
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }
    if (formatContext) {
        if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatContext->pb);
        }
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }

    opened = false;
}

void FfmpegEncoder::writePacket(AVPacket *packet) {
    av_packet_rescale_ts(packet, codecContext->time_base, stream->time_base);
    packet->stream_index = stream->index;
    int err = av_interleaved_write_frame(formatContext, packet);
    if (err < 0) {
        throw std::runtime_error("Failed to write packet: " +
                                 ffmpegErrStr(err));
    }
}
} // namespace ffmpeg_utils
