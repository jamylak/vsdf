#include "ffmpeg_encoder.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstring>

FFmpegEncoder::FFmpegEncoder(const std::string &outputPath, uint32_t width,
                             uint32_t height, uint32_t fps)
    : width(width), height(height), fps(fps), outputPath(outputPath) {}

FFmpegEncoder::~FFmpegEncoder() { cleanup(); }

void FFmpegEncoder::cleanup() {
    if (packet) {
        av_packet_free(&packet);
        packet = nullptr;
    }
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }
    if (formatContext) {
        if (initialized && formatContext->pb) {
            avio_closep(&formatContext->pb);
        }
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
    initialized = false;
}

void FFmpegEncoder::initialize() {
    if (initialized) {
        throw std::runtime_error("FFmpegEncoder already initialized");
    }

    spdlog::info("Initializing FFmpeg encoder for output: {}", outputPath);
    spdlog::info("Video dimensions: {}x{} @ {} fps", width, height, fps);

    // Allocate format context
    int ret = avformat_alloc_output_context2(&formatContext, nullptr, nullptr,
                                             outputPath.c_str());
    if (ret < 0 || !formatContext) {
        throw std::runtime_error("Failed to allocate output format context");
    }

    // Find H.264 encoder
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        cleanup();
        throw std::runtime_error("H.264 codec not found");
    }

    // Create video stream
    videoStream = avformat_new_stream(formatContext, nullptr);
    if (!videoStream) {
        cleanup();
        throw std::runtime_error("Failed to create video stream");
    }
    videoStream->id = static_cast<int>(formatContext->nb_streams - 1);

    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        cleanup();
        throw std::runtime_error("Failed to allocate codec context");
    }

    // Set codec parameters
    codecContext->codec_id = AV_CODEC_ID_H264;
    codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    codecContext->width = static_cast<int>(width);
    codecContext->height = static_cast<int>(height);
    codecContext->time_base = AVRational{1, static_cast<int>(fps)};
    codecContext->framerate = AVRational{static_cast<int>(fps), 1};
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    codecContext->gop_size = 10;
    codecContext->max_b_frames = 1;

    // Set codec options for better quality
    av_opt_set(codecContext->priv_data, "preset", "medium", 0);
    av_opt_set(codecContext->priv_data, "crf", "23", 0);

    // Some formats want stream headers to be separate
    if (formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open codec
    ret = avcodec_open2(codecContext, codec, nullptr);
    if (ret < 0) {
        cleanup();
        throw std::runtime_error("Failed to open codec");
    }

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(videoStream->codecpar, codecContext);
    if (ret < 0) {
        cleanup();
        throw std::runtime_error("Failed to copy codec parameters to stream");
    }

    // Allocate frame
    frame = av_frame_alloc();
    if (!frame) {
        cleanup();
        throw std::runtime_error("Failed to allocate frame");
    }
    frame->format = codecContext->pix_fmt;
    frame->width = codecContext->width;
    frame->height = codecContext->height;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        cleanup();
        throw std::runtime_error("Failed to allocate frame buffer");
    }

    // Allocate packet
    packet = av_packet_alloc();
    if (!packet) {
        cleanup();
        throw std::runtime_error("Failed to allocate packet");
    }

    // Create SwsContext for RGBA to YUV420P conversion
    swsContext = sws_getContext(
        static_cast<int>(width), static_cast<int>(height), AV_PIX_FMT_RGBA,
        static_cast<int>(width), static_cast<int>(height), AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsContext) {
        cleanup();
        throw std::runtime_error("Failed to create SwsContext");
    }

    // Open output file
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&formatContext->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            cleanup();
            throw std::runtime_error("Failed to open output file: " + outputPath);
        }
    }

    // Write file header
    ret = avformat_write_header(formatContext, nullptr);
    if (ret < 0) {
        cleanup();
        throw std::runtime_error("Failed to write format header");
    }

    initialized = true;
    frameCount = 0;
    spdlog::info("FFmpeg encoder initialized successfully");
}

void FFmpegEncoder::encodeFrame(const uint8_t *rgbaData) {
    if (!initialized) {
        throw std::runtime_error("FFmpegEncoder not initialized");
    }

    // Make frame writable
    int ret = av_frame_make_writable(frame);
    if (ret < 0) {
        throw std::runtime_error("Failed to make frame writable");
    }

    // Convert RGBA to YUV420P
    const uint8_t *srcData[1] = {rgbaData};
    int srcLinesize[1] = {static_cast<int>(width * 4)};
    
    sws_scale(swsContext, srcData, srcLinesize, 0, static_cast<int>(height),
              frame->data, frame->linesize);

    // Set frame PTS (presentation timestamp)
    frame->pts = frameCount;

    // Send frame to encoder
    ret = avcodec_send_frame(codecContext, frame);
    if (ret < 0) {
        throw std::runtime_error("Failed to send frame to encoder");
    }

    // Receive and write encoded packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecContext, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            throw std::runtime_error("Failed to receive packet from encoder");
        }

        // Rescale packet timestamps
        av_packet_rescale_ts(packet, codecContext->time_base,
                            videoStream->time_base);
        packet->stream_index = videoStream->index;

        // Write packet to output file
        ret = av_interleaved_write_frame(formatContext, packet);
        av_packet_unref(packet);
        
        if (ret < 0) {
            throw std::runtime_error("Failed to write packet to output file");
        }
    }

    frameCount++;
}

void FFmpegEncoder::finalize() {
    if (!initialized) {
        return;
    }

    spdlog::info("Finalizing video encoding, {} frames encoded", frameCount);

    // Flush encoder
    int ret = avcodec_send_frame(codecContext, nullptr);
    if (ret >= 0) {
        while (ret >= 0) {
            ret = avcodec_receive_packet(codecContext, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret >= 0) {
                av_packet_rescale_ts(packet, codecContext->time_base,
                                    videoStream->time_base);
                packet->stream_index = videoStream->index;
                av_interleaved_write_frame(formatContext, packet);
                av_packet_unref(packet);
            }
        }
    }

    // Write trailer
    av_write_trailer(formatContext);

    spdlog::info("Video encoding completed: {}", outputPath);
    cleanup();
}
