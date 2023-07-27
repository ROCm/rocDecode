/*
Copyright (c) 2023 - 2023 Advanced Micro Devices, Inc. All rights reserved.

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
    #include <libavutil/pixdesc.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/hwcontext_vaapi.h>
    #include <va/va.h>
    #include <va/va_drmcommon.h>
}

// Video Demuxer Interface class
class VideoDemuxer {
    public:
        class StreamProvider {
            public:
                virtual ~StreamProvider() {}
                virtual int getData(uint8_t *pBuf, int nBuf) = 0;
        };
        VideoDemuxer(const char *inputFilePath) : VideoDemuxer(createFmtContextUtil(inputFilePath)) {}
        VideoDemuxer(StreamProvider *pStreamProvider) : VideoDemuxer(createFmtContextUtil(pStreamProvider)) {av_io_ctx_ = av_fmt_input_ctx_->pb;}
        ~VideoDemuxer();
        bool demux(uint8_t **ppVideo, int *pnVideoBytes, int64_t *pts = nullptr);
    private:
        VideoDemuxer(AVFormatContext *av_fmt_input_ctx_);
        AVFormatContext *createFmtContextUtil(StreamProvider *pStreamProvider);
        AVFormatContext *createFmtContextUtil(const char *inputFilePath);
        static int readPacket(void *pData, uint8_t *pBuf, int nBuf);
        AVFormatContext *av_fmt_input_ctx_ = nullptr;
        AVIOContext *av_io_ctx_ = nullptr;
        AVPacket* packet_ = nullptr;
        AVPacket* packetFiltered_ = nullptr;
        AVBSFContext *av_bsf_ctx_ = nullptr;
        AVCodecID av_videoCodecID_;
        uint8_t *pDataWithHeader = nullptr;
        int av_stream_ = 0;
        bool isH264_ = false; 
        bool isHEVC_ = false;
        bool isMPEG4_ = false;
        int64_t defaultTimeScale = 1000;
        double timeBase = 0.0;
        unsigned int frameCnt = 0;
};

VideoDemuxer::~VideoDemuxer() {
    if (!av_fmt_input_ctx_) {
        return;
    }
    if (packet_) {
        av_packet_free(&packet_);
    }
    if (packetFiltered_) {
        av_packet_free(&packetFiltered_);
    }
    if (av_bsf_ctx_) {
        av_bsf_free(&av_bsf_ctx_);
    }
    avformat_close_input(&av_fmt_input_ctx_);
    if (av_io_ctx_) {
        av_freep(&av_io_ctx_->buffer);
        av_freep(&av_io_ctx_);
    }
    if (pDataWithHeader) {
        av_free(pDataWithHeader);
    }
}

bool VideoDemuxer::demux(uint8_t **ppVideo, int *pnVideoBytes, int64_t *pts) {
    if (!av_fmt_input_ctx_) {
        return false;
    }
    *pnVideoBytes = 0;
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
    if (isH264_ || isHEVC_) {
        if (packetFiltered_->data) {
            av_packet_unref(packetFiltered_);
        }
        if (av_bsf_send_packet(av_bsf_ctx_, packet_) != 0) {
            std::cerr << "ERROR: av_bsf_send_packet failed!" << std::endl;
            return false;
        }
        if (av_bsf_receive_packet(av_bsf_ctx_, packetFiltered_) != 0) {
            std::cerr << "ERROR: av_bsf_receive_packet failed!" << std::endl;
            return false;
        }
        *ppVideo = packetFiltered_->data;
        *pnVideoBytes = packetFiltered_->size;
        if (pts)
            *pts = (int64_t) (packetFiltered_->pts * defaultTimeScale * timeBase);
        } else {
           if (isMPEG4_ && (frameCnt == 0)) {
               int extDataSize = av_fmt_input_ctx_->streams[av_stream_]->codecpar->extradata_size;
               if (extDataSize > 0) {
                    pDataWithHeader = (uint8_t *)av_malloc(extDataSize + packet_->size - 3 * sizeof(uint8_t));
                    if (!pDataWithHeader) {
                        std::cerr << "ERROR: av_malloc failed!" << std::endl;
                        return false;
                    }
                    memcpy(pDataWithHeader, av_fmt_input_ctx_->streams[av_stream_]->codecpar->extradata, extDataSize);
                    memcpy(pDataWithHeader + extDataSize, packet_->data + 3, packet_->size - 3 * sizeof(uint8_t));
                    *ppVideo = pDataWithHeader;
                    *pnVideoBytes = extDataSize + packet_->size - 3 * sizeof(uint8_t);
                }
            } else {
                *ppVideo = packet_->data;
                *pnVideoBytes = packet_->size;
            }
            if (pts)
                *pts = (int64_t)(packet_->pts * defaultTimeScale * timeBase);
    }
    frameCnt++;
    return true;
}

VideoDemuxer::VideoDemuxer(AVFormatContext *av_fmt_input_ctx_) : av_fmt_input_ctx_(av_fmt_input_ctx_) {
    if (!av_fmt_input_ctx_) {
        std::cerr << "ERROR: av_fmt_input_ctx_ is not vaild!" << std::endl;
        return;
    }
    packet_ = av_packet_alloc();
    packetFiltered_ = av_packet_alloc();
    if (!packet_ || !packetFiltered_) {
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
        av_packet_free(&packetFiltered_);
        return;
    }
    av_videoCodecID_ = av_fmt_input_ctx_->streams[av_stream_]->codecpar->codec_id;
    AVRational rTimeBase = av_fmt_input_ctx_->streams[av_stream_]->time_base;
    timeBase = av_q2d(rTimeBase);

    isH264_ = av_videoCodecID_ == AV_CODEC_ID_H264 && (!strcmp(av_fmt_input_ctx_->iformat->long_name, "QuickTime / MOV") 
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "FLV (Flash Video)") 
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "Matroska / WebM"));
    isHEVC_ = av_videoCodecID_ == AV_CODEC_ID_HEVC && (!strcmp(av_fmt_input_ctx_->iformat->long_name, "QuickTime / MOV")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "FLV (Flash Video)")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "Matroska / WebM"));
    isMPEG4_ = av_videoCodecID_ == AV_CODEC_ID_MPEG4 && (!strcmp(av_fmt_input_ctx_->iformat->long_name, "QuickTime / MOV")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "FLV (Flash Video)")
                || !strcmp(av_fmt_input_ctx_->iformat->long_name, "Matroska / WebM"));

    if (isH264_) {
        const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
        if (!bsf) {
            std::cerr << "ERROR: av_bsf_get_by_name() failed" << std::endl;
            av_packet_free(&packet_);
            av_packet_free(&packetFiltered_);
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
    if (isHEVC_) {
        const AVBitStreamFilter *bsf = av_bsf_get_by_name("hevc_mp4toannexb");
        if (!bsf) {
            std::cerr << "ERROR: av_bsf_get_by_name() failed" << std::endl;
            av_packet_free(&packet_);
            av_packet_free(&packetFiltered_);
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

AVFormatContext *VideoDemuxer::createFmtContextUtil(StreamProvider *pStreamProvider) {
    AVFormatContext *ctx = nullptr;
    if (!(ctx = avformat_alloc_context())) {
        std::cerr << "ERROR: avformat_alloc_context failed" << std::endl;
        return nullptr;
    }
    uint8_t *avioc_buffer = nullptr;
    int avioc_buffer_size = 8 * 1024 * 1024;
    avioc_buffer = (uint8_t *)av_malloc(avioc_buffer_size);
    if (!avioc_buffer) {
        std::cerr << "ERROR: av_malloc failed!" << std::endl;;
        return nullptr;
    }
    av_io_ctx_ = avio_alloc_context(avioc_buffer, avioc_buffer_size,
        0, pStreamProvider, &readPacket, nullptr, nullptr);
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

AVFormatContext *VideoDemuxer::createFmtContextUtil(const char *inputFilePath) {
    avformat_network_init();
    av_log_set_level(AV_LOG_QUIET);
    AVFormatContext *ctx = nullptr;
    if (avformat_open_input(&ctx, inputFilePath, nullptr, nullptr) != 0 ) {
        std::cerr << "ERROR: avformat_open_input failed!" << std::endl;
        return nullptr;
    }
    return ctx;
}

int VideoDemuxer::readPacket(void *pData, uint8_t *pBuf, int nBuf) {
    return ((StreamProvider *)pData)->getData(pBuf, nBuf);
}
