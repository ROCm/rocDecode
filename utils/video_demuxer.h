/*
Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#include <iostream>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

#include "rocdecode.h"
/*!
 * \file
 * \brief The AMD Video Demuxer for rocDecode Library.
 *
 * \defgroup group_amd_rocdecode_videodemuxer videoDemuxer: AMD rocDecode Video Demuxer API
 * \brief AMD The rocDecode video demuxer API.
 */


// Video Demuxer Interface class
class VideoDemuxer {
    public:
        class StreamProvider {
            public:
                virtual ~StreamProvider() {}
                virtual int GetData(uint8_t *buf, int buf_size) = 0;
        };
        AVCodecID GetCodecID() { return av_video_codec_id_; }
        AVPixelFormat GetChromaFormat() { return chroma_format_; }
        void GetCodedDims(int *width, int *height) {
            *width = coded_width_;
            *height = coded_height_;
        }
        int GetBitDepth() {
            return bit_depth_;
        }
        int GetFrameSize() {
            return coded_width_ * (coded_height_ + chroma_height_) * bytes_per_pixel_;
        }

        VideoDemuxer(const char *input_file_path) : VideoDemuxer(CreateFmtContextUtil(input_file_path)) {}
        VideoDemuxer(StreamProvider *stream_provider) : VideoDemuxer(CreateFmtContextUtil(stream_provider)) {av_io_ctx_ = av_fmt_input_ctx_->pb;}
        ~VideoDemuxer();
        bool Demux(uint8_t **video, int *video_size, int64_t *pts = nullptr);
    private:
        VideoDemuxer(AVFormatContext *av_fmt_input_ctx);
        AVFormatContext *CreateFmtContextUtil(StreamProvider *stream_provider);
        AVFormatContext *CreateFmtContextUtil(const char *input_file_path);
        static int ReadPacket(void *data, uint8_t *buf, int buf_size);
        AVFormatContext *av_fmt_input_ctx_ = nullptr;
        AVIOContext *av_io_ctx_ = nullptr;
        AVPacket* packet_ = nullptr;
        AVPacket* packet_filtered_ = nullptr;
        AVBSFContext *av_bsf_ctx_ = nullptr;
        AVCodecID av_video_codec_id_;
        AVPixelFormat chroma_format_;
        int coded_width_, coded_height_, chroma_height_;
        int bit_depth_, bytes_per_pixel_;
        uint8_t *data_with_header_ = nullptr;
        int av_stream_ = 0;
        bool is_h264_ = false; 
        bool is_hevc_ = false;
        bool is_mpeg4_ = false;
        int64_t default_time_scale_ = 1000;
        double time_base_ = 0.0;
        unsigned int frame_count_ = 0;
};

VideoDemuxer::~VideoDemuxer() {
    if (!av_fmt_input_ctx_) {
        return;
    }
    if (packet_) {
        av_packet_free(&packet_);
    }
    if (packet_filtered_) {
        av_packet_free(&packet_filtered_);
    }
    if (av_bsf_ctx_) {
        av_bsf_free(&av_bsf_ctx_);
    }
    avformat_close_input(&av_fmt_input_ctx_);
    if (av_io_ctx_) {
        av_freep(&av_io_ctx_->buffer);
        av_freep(&av_io_ctx_);
    }
    if (data_with_header_) {
        av_free(data_with_header_);
    }
}

bool VideoDemuxer::Demux(uint8_t **video, int *video_size, int64_t *pts) {
    if (!av_fmt_input_ctx_) {
        return false;
    }
    *video_size = 0;
    if (packet_->data) {
        av_packet_unref(packet_);
    }
    int ret = 0;
    while ((ret = av_read_frame(av_fmt_input_ctx_, packet_)) >= 0 && packet_->stream_index != av_stream_) {
        av_packet_unref(packet_);
    }
    if (ret < 0) {
        return false;
    }
    if (is_h264_ || is_hevc_) {
        if (packet_filtered_->data) {
            av_packet_unref(packet_filtered_);
        }
        if (av_bsf_send_packet(av_bsf_ctx_, packet_) != 0) {
            std::cerr << "ERROR: av_bsf_send_packet failed!" << std::endl;
            return false;
        }
        if (av_bsf_receive_packet(av_bsf_ctx_, packet_filtered_) != 0) {
            std::cerr << "ERROR: av_bsf_receive_packet failed!" << std::endl;
            return false;
        }
        *video = packet_filtered_->data;
        *video_size = packet_filtered_->size;
        if (pts)
            *pts = (int64_t) (packet_filtered_->pts * default_time_scale_ * time_base_);
        } else {
           if (is_mpeg4_ && (frame_count_ == 0)) {
               int ext_data_size = av_fmt_input_ctx_->streams[av_stream_]->codecpar->extradata_size;
               if (ext_data_size > 0) {
                    data_with_header_ = (uint8_t *)av_malloc(ext_data_size + packet_->size - 3 * sizeof(uint8_t));
                    if (!data_with_header_) {
                        std::cerr << "ERROR: av_malloc failed!" << std::endl;
                        return false;
                    }
                    memcpy(data_with_header_, av_fmt_input_ctx_->streams[av_stream_]->codecpar->extradata, ext_data_size);
                    memcpy(data_with_header_ + ext_data_size, packet_->data + 3, packet_->size - 3 * sizeof(uint8_t));
                    *video = data_with_header_;
                    *video_size = ext_data_size + packet_->size - 3 * sizeof(uint8_t);
                }
            } else {
                *video = packet_->data;
                *video_size = packet_->size;
            }
            if (pts)
                *pts = (int64_t)(packet_->pts * default_time_scale_ * time_base_);
    }
    frame_count_++;
    return true;
}

VideoDemuxer::VideoDemuxer(AVFormatContext *av_fmt_input_ctx) : av_fmt_input_ctx_(av_fmt_input_ctx) {
    av_log_set_level(AV_LOG_QUIET);
    if (!av_fmt_input_ctx_) {
        std::cerr << "ERROR: av_fmt_input_ctx_ is not vaild!" << std::endl;
        return;
    }
    packet_ = av_packet_alloc();
    packet_filtered_ = av_packet_alloc();
    if (!packet_ || !packet_filtered_) {
        std::cerr << "ERROR: av_packet_alloc failed!" << std::endl;
        return;
    }
    if (avformat_find_stream_info(av_fmt_input_ctx_, nullptr) < 0) {
        std::cerr << "ERROR: avformat_find_stream_info failed!" << std::endl;
        return;
    }
    av_stream_ = av_find_best_stream(av_fmt_input_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (av_stream_ < 0) {
        std::cerr << "ERROR: av_find_best_stream failed!" << std::endl;
        av_packet_free(&packet_);
        av_packet_free(&packet_filtered_);
        return;
    }
    av_video_codec_id_ = av_fmt_input_ctx_->streams[av_stream_]->codecpar->codec_id;
    coded_width_ = av_fmt_input_ctx_->streams[av_stream_]->codecpar->width;
    coded_height_ = av_fmt_input_ctx_->streams[av_stream_]->codecpar->height;
    chroma_format_ = (AVPixelFormat)av_fmt_input_ctx_->streams[av_stream_]->codecpar->format;
    AVRational time_base = av_fmt_input_ctx_->streams[av_stream_]->time_base;
    time_base_ = av_q2d(time_base);
    // Set bit depth, chroma height, bits per pixel based on chroma_format_ of input
    switch (chroma_format_)
    {
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_GRAY10LE:   // monochrome is treated as 420 with chroma filled with 0x0
        bit_depth_ = 10;
        chroma_height_ = (coded_height_ + 1) >> 1;
        bytes_per_pixel_ = 2;
        break;
    case AV_PIX_FMT_YUV420P12LE:
        bit_depth_ = 12;
        chroma_height_ = (coded_height_ + 1) >> 1;
        bytes_per_pixel_ = 2;
        break;
    case AV_PIX_FMT_YUV444P10LE:
        bit_depth_ = 10;
        chroma_height_ = coded_height_ << 1;
        bytes_per_pixel_ = 2;
        break;
    case AV_PIX_FMT_YUV444P12LE:
        bit_depth_ = 12;
        chroma_height_ = coded_height_ << 1;
        bytes_per_pixel_ = 2;
        break;
    case AV_PIX_FMT_YUV444P:
        bit_depth_ = 8;
        chroma_height_ = coded_height_ << 1;
        bytes_per_pixel_ = 1;
        break;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUVJ422P:   // jpeg decoder output is subsampled to NV12 for 422/444 so treat it as 420
    case AV_PIX_FMT_YUVJ444P:   // jpeg decoder output is subsampled to NV12 for 422/444 so treat it as 420
    case AV_PIX_FMT_GRAY8:      // monochrome is treated as 420 with chroma filled with 0x0
        bit_depth_ = 8;
        chroma_height_ = (coded_height_ + 1) >> 1;
        bytes_per_pixel_ = 1;
        break;
    default:
        std::cerr << "Warning: ChromaFormat not recognized. Assuming 420" << std::endl;
        chroma_format_ = AV_PIX_FMT_YUV420P;
        bit_depth_ = 8;
        chroma_height_ = (coded_height_ + 1) >> 1;
        bytes_per_pixel_ = 1;
    }

    is_h264_ = av_video_codec_id_ == AV_CODEC_ID_H264 && (!strcmp(av_fmt_input_ctx_->iformat->long_name, "QuickTime / MOV") 
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "FLV (Flash Video)") 
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "Matroska / WebM"));
    is_hevc_ = av_video_codec_id_ == AV_CODEC_ID_HEVC && (!strcmp(av_fmt_input_ctx_->iformat->long_name, "QuickTime / MOV")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "FLV (Flash Video)")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "Matroska / WebM"));
    is_mpeg4_ = av_video_codec_id_ == AV_CODEC_ID_MPEG4 && (!strcmp(av_fmt_input_ctx_->iformat->long_name, "QuickTime / MOV")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "FLV (Flash Video)")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "Matroska / WebM"));

    if (is_h264_) {
        const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
        if (!bsf) {
            std::cerr << "ERROR: av_bsf_get_by_name() failed" << std::endl;
            av_packet_free(&packet_);
            av_packet_free(&packet_filtered_);
            return;
        }
        if (av_bsf_alloc(bsf, &av_bsf_ctx_) != 0) {
            std::cerr << "ERROR: av_bsf_alloc failed!" << std::endl;
                return;
        }
        avcodec_parameters_copy(av_bsf_ctx_->par_in, av_fmt_input_ctx_->streams[av_stream_]->codecpar);
        if (av_bsf_init(av_bsf_ctx_) < 0) {
            std::cerr << "ERROR: av_bsf_init failed!" << std::endl;
            return;
        }
    }
    if (is_hevc_) {
        const AVBitStreamFilter *bsf = av_bsf_get_by_name("hevc_mp4toannexb");
        if (!bsf) {
            std::cerr << "ERROR: av_bsf_get_by_name() failed" << std::endl;
            av_packet_free(&packet_);
            av_packet_free(&packet_filtered_);
            return;
        }
        if (av_bsf_alloc(bsf, &av_bsf_ctx_) != 0 ) {
            std::cerr << "ERROR: av_bsf_alloc failed!" << std::endl;
            return;
        }
        avcodec_parameters_copy(av_bsf_ctx_->par_in, av_fmt_input_ctx_->streams[av_stream_]->codecpar);
        if (av_bsf_init(av_bsf_ctx_) < 0) {
            std::cerr << "ERROR: av_bsf_init failed!" << std::endl;
            return;
        }
    }
}

AVFormatContext *VideoDemuxer::CreateFmtContextUtil(StreamProvider *stream_provider) {
    AVFormatContext *ctx = nullptr;
    if (!(ctx = avformat_alloc_context())) {
        std::cerr << "ERROR: avformat_alloc_context failed" << std::endl;
        return nullptr;
    }
    uint8_t *avioc_buffer = nullptr;
    int avioc_buffer_size = 100 * 1024 * 1024;
    avioc_buffer = (uint8_t *)av_malloc(avioc_buffer_size);
    if (!avioc_buffer) {
        std::cerr << "ERROR: av_malloc failed!" << std::endl;
        return nullptr;
    }
    av_io_ctx_ = avio_alloc_context(avioc_buffer, avioc_buffer_size,
        0, stream_provider, &ReadPacket, nullptr, nullptr);
    if (!av_io_ctx_) {
        std::cerr << "ERROR: avio_alloc_context failed!" << std::endl;
        return nullptr;
    }
    ctx->pb = av_io_ctx_;

    if (avformat_open_input(&ctx, nullptr, nullptr, nullptr) != 0) {
        std::cerr << "ERROR: avformat_open_input failed!" << std::endl;
        return nullptr;
    }
    return ctx;
}

AVFormatContext *VideoDemuxer::CreateFmtContextUtil(const char *input_file_path) {
    avformat_network_init();
    AVFormatContext *ctx = nullptr;
    if (avformat_open_input(&ctx, input_file_path, nullptr, nullptr) != 0 ) {
        std::cerr << "ERROR: avformat_open_input failed!" << std::endl;
        return nullptr;
    }
    return ctx;
}

int VideoDemuxer::ReadPacket(void *data, uint8_t *buf, int buf_size) {
    return ((StreamProvider *)data)->GetData(buf, buf_size);
}

static inline rocDecVideoCodec AVCodec2RocDecVideoCodec(AVCodecID av_codec) {
    switch (av_codec) {
    case AV_CODEC_ID_MPEG1VIDEO : return rocDecVideoCodec_MPEG1;
    case AV_CODEC_ID_MPEG2VIDEO : return rocDecVideoCodec_MPEG2;
    case AV_CODEC_ID_MPEG4      : return rocDecVideoCodec_MPEG4;
    case AV_CODEC_ID_H264       : return rocDecVideoCodec_AVC;
    case AV_CODEC_ID_HEVC       : return rocDecVideoCodec_HEVC;
    case AV_CODEC_ID_VP8        : return rocDecVideoCodec_VP8;
    case AV_CODEC_ID_VP9        : return rocDecVideoCodec_VP9;
    case AV_CODEC_ID_MJPEG      : return rocDecVideoCodec_JPEG;
    case AV_CODEC_ID_AV1        : return rocDecVideoCodec_AV1;
    default                     : return rocDecVideoCodec_NumCodecs;
    }
}