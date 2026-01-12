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

FfmpegEncoder::FfmpegEncoder(const EncodeSettings &settings, int width,
                             int height, AVPixelFormat srcFormat, int srcStride)
    : settings(settings), width(width), height(height), srcFormat(srcFormat),
      srcStride(srcStride) {}

FfmpegEncoder::~FfmpegEncoder() { close(); }

void FfmpegEncoder::open() {
    if (opened)
        return;

    if (settings.outputPath.empty())
        throw std::runtime_error("FFmpeg output path is empty");

    const AVCodec *codec = avcodec_find_encoder_by_name(settings.codec.c_str());
    if (!codec)
        throw std::runtime_error("Failed to find encoder: " + settings.codec);

    // Let FFmpeg infer container format from output path (extension).
    int err = avformat_alloc_output_context2(&formatContext, nullptr, nullptr,
                                             settings.outputPath.c_str());
    if (err < 0 || !formatContext)
        throw std::runtime_error("Failed to create output context: " +
                                 ffmpegErrStr(err));

    // Create one logical track in the container (video-only in this case).
    stream = avformat_new_stream(formatContext, nullptr);
    if (!stream)
        throw std::runtime_error("Failed to create output stream");

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext)
        throw std::runtime_error("Failed to allocate codec context");

    codecContext->codec_id = codec->id;
    codecContext->width = width;
    codecContext->height = height;
    // Encoder timestamps are in 1/fps timebase for frame-accurate PTS.
    codecContext->time_base = AVRational{1, settings.fps};
    codecContext->framerate = AVRational{settings.fps, 1};
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    codecContext->gop_size = settings.fps;
    // SDF values are full-range 0-255; signal full-range (JPEG) in bitstream.
    codecContext->color_range = AVCOL_RANGE_JPEG;

    // Some containers require extradata in the stream header instead of
    // packets.
    if (formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Optional codec knobs (e.g., libx264 preset + CRF quality target).
    if (!settings.preset.empty()) {
        err = av_opt_set(codecContext->priv_data, "preset",
                         settings.preset.c_str(), 0);
        if (err < 0)
            spdlog::warn("FFmpeg preset option rejected: {}",
                         ffmpegErrStr(err));
    }

    if (settings.crf >= 0) {
        err = av_opt_set_int(codecContext->priv_data, "crf", settings.crf, 0);
        if (err < 0)
            spdlog::warn("FFmpeg CRF option rejected: {}", ffmpegErrStr(err));
    }

    // Initializes codec internals and validates parameters.
    err = avcodec_open2(codecContext, codec, nullptr);
    if (err < 0)
        throw std::runtime_error("Failed to open encoder: " +
                                 ffmpegErrStr(err));

    // Copy encoder settings into the container stream header metadata.
    err = avcodec_parameters_from_context(stream->codecpar, codecContext);
    if (err < 0)
        throw std::runtime_error("Failed to set stream params: " +
                                 ffmpegErrStr(err));

    // Match stream time base to the encoder for correct PTS/DTS units.
    stream->time_base = codecContext->time_base;
    // Advertise the intended average frame rate for container metadata.
    stream->avg_frame_rate = codecContext->framerate;
    // Set the nominal frame rate used by muxers/readers.
    stream->r_frame_rate = codecContext->framerate;

    // We always write to a file, but just keep this anyway...
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        err = avio_open(&formatContext->pb, settings.outputPath.c_str(),
                        AVIO_FLAG_WRITE);
        if (err < 0)
            throw std::runtime_error("Failed to open output: " +
                                     ffmpegErrStr(err));
    }

    // Write container header and muxer metadata to the output sink.
    // MP4: writes ftyp + moov boxes (initial container metadata) before mdat.
    //      [ ftyp ][ moov ][ mdat ] ... (unless fragmented MP4)
    err = avformat_write_header(formatContext, nullptr);
    if (err < 0)
        throw std::runtime_error("Failed to write header: " +
                                 ffmpegErrStr(err));

    // Frame buffer that matches the encoder's expected pixel format.
    dstFrame = av_frame_alloc();
    if (!dstFrame)
        throw std::runtime_error("Failed to allocate destination frame");

    dstFrame->format = codecContext->pix_fmt;
    dstFrame->width = width;
    dstFrame->height = height;
    // Ensure the frame metadata matches the full-range encoder setting.
    dstFrame->color_range = AVCOL_RANGE_JPEG;

    // Let FFmpeg choose a default alignment for this build/CPU.
    err = av_frame_get_buffer(dstFrame, 0);
    if (err < 0)
        throw std::runtime_error("Failed to allocate frame buffer: " +
                                 ffmpegErrStr(err));

    // ASCII sketch of a tiny 4×4 YUV420P frame:
    //
    //   Plane 0 (Y, 4x4):
    //   Y00 Y01 Y02 Y03
    //   Y10 Y11 Y12 Y13
    //   Y20 Y21 Y22 Y23
    //   Y30 Y31 Y32 Y33
    //
    //   Plane 1 (U, 2x2):
    //   U00 U01
    //   U10 U11
    //
    //   Plane 2 (V, 2x2):
    //   V00 V01
    //   V10 V11
    //
    //   What are data[0..4]?
    //   - data[0]: pointer to plane 0 (first byte of Y or packed RGB)
    //   - data[1]: pointer to plane 1 (U/Cb if planar)
    //   - data[2]: pointer to plane 2 (V/Cr if planar)
    //   - data[3]: pointer to plane 3 (alpha or extra plane if format uses it)
    //   - data[4]: usually unused (FFmpeg reserves up to 8 pointers via
    //   AV_NUM_DATA_POINTERS)
    //
    //   linesize[i] = number of bytes per row in plane i

    // Frame wrapper for the caller's input (no allocation for src data).
    //   - Packed RGB (e.g., AV_PIX_FMT_RGB24): plane 0 = interleaved RGBRGB...
    //     (no Y/UV planes at all, plane 1-3 not used)
    //   - Planar YUV (e.g., AV_PIX_FMT_YUV420P): plane 0 = Y, plane 1 = U, plane 2 = V

    srcFrame = av_frame_alloc();
    if (!srcFrame)
        throw std::runtime_error("Failed to allocate source frame");
    srcFrame->format = srcFormat;
    srcFrame->width = width;
    srcFrame->height = height;
    // Source pixels are full-range 0-255 before conversion.
    srcFrame->color_range = AVCOL_RANGE_JPEG;
    srcFrame->data[1] = nullptr;
    srcFrame->data[2] = nullptr;
    srcFrame->data[3] = nullptr;
    srcFrame->linesize[1] = 0;
    srcFrame->linesize[2] = 0;
    srcFrame->linesize[3] = 0;

    // Create a colorspace/format conversion context for src -> encoder format.
    swsContext = sws_getCachedContext(nullptr, width, height, srcFormat, width,
                                      height, codecContext->pix_fmt,
                                      SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!swsContext)
        throw std::runtime_error("Failed to create sws context");

    packet = av_packet_alloc();
    if (!packet)
        throw std::runtime_error("Failed to allocate packet");

    opened = true;
}

void FfmpegEncoder::encodeFrame(const uint8_t *srcData, int64_t frameIndex) {
    if (!opened)
        throw std::runtime_error("FFmpeg encoder not opened");

    srcFrame->data[0] = const_cast<uint8_t *>(srcData);
    srcFrame->linesize[0] = srcStride;

    // Ensure dstFrame has an owned, writable buffer before conversion.
    int err = av_frame_make_writable(dstFrame);
    if (err < 0)
        throw std::runtime_error("Failed to make frame writable: " +
                                 ffmpegErrStr(err));

    // Convert/copy into the destination frame in the encoder's pixel format.
    sws_scale(swsContext, srcFrame->data, srcFrame->linesize, 0, height,
              dstFrame->data, dstFrame->linesize);

    // PTS in stream timebase units; duration set to one frame.
    dstFrame->pts = frameIndex;
    dstFrame->duration = 1;

    // Push one frame into the encoder; it may output 0..N packets.
    err = avcodec_send_frame(codecContext, dstFrame);
    if (err < 0)
        throw std::runtime_error("Failed to send frame: " + ffmpegErrStr(err));

    // Drain all packets produced for the submitted frame.
    while (true) {
        err = avcodec_receive_packet(codecContext, packet);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            break;
        if (err < 0) {
            av_packet_free(&packet);
            throw std::runtime_error("Failed to receive packet: " +
                                     ffmpegErrStr(err));
        }
        writePacket(packet);
        av_packet_unref(packet);
    }
}

void FfmpegEncoder::flush() {
    if (!opened)
        return;

    // Send a null frame to signal end-of-stream and flush delayed frames.
    int err = avcodec_send_frame(codecContext, nullptr);
    if (err < 0)
        throw std::runtime_error("Failed to flush encoder: " +
                                 ffmpegErrStr(err));

    // Drain any remaining packets buffered by the encoder.
    while (true) {
        err = avcodec_receive_packet(codecContext, packet);
        if (err == AVERROR_EOF || err == AVERROR(EAGAIN))
            break;
        if (err < 0) {
            av_packet_free(&packet);
            throw std::runtime_error("Failed to drain packet: " +
                                     ffmpegErrStr(err));
        }
        writePacket(packet);
        av_packet_unref(packet);
    }
}

void FfmpegEncoder::close() noexcept {
    if (!opened)
        return;

    // Finalize the container (MP4: write/close moov if needed, etc.).
    int err = av_write_trailer(formatContext);
    if (err < 0)
        spdlog::warn("Failed to write trailer: {}", ffmpegErrStr(err));

    if (dstFrame)
        av_frame_free(&dstFrame);
    if (srcFrame)
        av_frame_free(&srcFrame);
    if (packet)
        av_packet_free(&packet);
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (codecContext)
        avcodec_free_context(&codecContext);
    if (formatContext) {
        if (!(formatContext->oformat->flags & AVFMT_NOFILE))
            avio_closep(&formatContext->pb);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }

    opened = false;
}

void FfmpegEncoder::writePacket(AVPacket *packet) {
    // Rescale from codec timebase to stream timebase before muxing.
    av_packet_rescale_ts(packet, codecContext->time_base, stream->time_base);
    packet->stream_index = stream->index;
    int err = av_interleaved_write_frame(formatContext, packet);
    if (err < 0)
        throw std::runtime_error("Failed to write packet: " +
                                 ffmpegErrStr(err));
}
} // namespace ffmpeg_utils
