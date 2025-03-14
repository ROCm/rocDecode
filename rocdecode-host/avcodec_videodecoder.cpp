/*
Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.

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

#include "avcodec_videodecoder.h"

/**
 * @brief helper function for inferring AVCodecID from rocDecVideoCodec
 * 
 * @param rocdec_codec 
 * @return AVCodecID 
 */
static inline AVCodecID RocDecVideoCodec2AVCodec(rocDecVideoCodec rocdec_codec) {
    switch (rocdec_codec) {
        case rocDecVideoCodec_MPEG1 : return AV_CODEC_ID_MPEG1VIDEO;
        case rocDecVideoCodec_MPEG2 : return AV_CODEC_ID_MPEG2VIDEO;
        case rocDecVideoCodec_MPEG4 : return AV_CODEC_ID_MPEG4;
        case rocDecVideoCodec_AVC   : return AV_CODEC_ID_H264;
        case rocDecVideoCodec_HEVC  : return AV_CODEC_ID_HEVC;
        case rocDecVideoCodec_VP8   : return AV_CODEC_ID_VP8;
        case rocDecVideoCodec_VP9   : return AV_CODEC_ID_VP9;
        case rocDecVideoCodec_JPEG  : return AV_CODEC_ID_MJPEG;
        case rocDecVideoCodec_AV1   : return AV_CODEC_ID_AV1;
        default                     : return AV_CODEC_ID_NONE;
    }
}

static inline float GetChromaWidthFactor(rocDecVideoSurfaceFormat surface_format) {
    float factor = 0.5;
    switch (surface_format) {
    case rocDecVideoSurfaceFormat_NV12:
    case rocDecVideoSurfaceFormat_P016:
        factor = 1.0;
        break;
    case rocDecVideoSurfaceFormat_YUV444:
    case rocDecVideoSurfaceFormat_YUV444_16Bit:
        factor = 1.0;
        break;
    case rocDecVideoSurfaceFormat_YUV420:
    case rocDecVideoSurfaceFormat_YUV420_16Bit:
        factor = 0.5;
        break;
    case rocDecVideoSurfaceFormat_YUV422:
    case rocDecVideoSurfaceFormat_YUV422_16Bit:
        factor = 0.5;
        break;
    }
    return factor;
};

/**
 * @brief helper function for inferring AVCodecID from rocDecVideoSurfaceFormat
 * 
 * @param rocdec_codec 
 * @return AVCodecID 
 */
static inline rocDecVideoSurfaceFormat AVPixelFormat2rocDecVideoSurfaceFormat(AVPixelFormat av_pixel_format) {
    switch (av_pixel_format) {
        case AV_PIX_FMT_YUV420P : 
        case AV_PIX_FMT_YUVJ420P : 
            return rocDecVideoSurfaceFormat_YUV420;
        case AV_PIX_FMT_YUV444P : 
        case AV_PIX_FMT_YUVJ444P : 
            return rocDecVideoSurfaceFormat_YUV444;
        case AV_PIX_FMT_YUV420P10LE :
        case AV_PIX_FMT_YUV420P12LE :
            return rocDecVideoSurfaceFormat_YUV420_16Bit;
        default :
            std::cerr << "ERROR: " << av_get_pix_fmt_name(av_pixel_format) << " pixel_format is not supported!" << std::endl;          
            return rocDecVideoSurfaceFormat_NV12;       // for sanity
    }
}

/**
 * @brief helper function for inferring AVCodecID from rocDecVideoSurfaceFormat
 * 
 * @param rocdec_codec 
 * @return AVCodecID 
 */
static inline rocDecVideoChromaFormat AVPixelFormat2rocDecVideoChromaFormat(AVPixelFormat av_pixel_format) {
    switch (av_pixel_format) {
        case AV_PIX_FMT_YUV420P : 
        case AV_PIX_FMT_YUVJ420P : 
        case AV_PIX_FMT_YUV420P10LE :
        case AV_PIX_FMT_YUV420P12LE :
            return rocDecVideoChromaFormat_420;
        case AV_PIX_FMT_YUV422P : 
        case AV_PIX_FMT_YUVJ422P :
            return rocDecVideoChromaFormat_422;
        case AV_PIX_FMT_YUV444P : 
        case AV_PIX_FMT_YUVJ444P : 
            return rocDecVideoChromaFormat_444;
        default :
            std::cerr << "ERROR: " << av_get_pix_fmt_name(av_pixel_format) << " pixel_format is not supported!" << std::endl;          
            return rocDecVideoChromaFormat_420;       // for sanity
    }
}

/**
 * @brief Constructor
 */

AvcodecVideoDecoder::AvcodecVideoDecoder(RocDecoderHostCreateInfo &decoder_create_info) : decoder_create_info_{decoder_create_info} {
    // start the avcodec decoding thread for multi-threading
    if (b_multithreading_) {
        ffmpeg_decoder_thread_ = new std::thread(&AvcodecVideoDecoder::DecodeThread, this);
        if (!ffmpeg_decoder_thread_) {
            THROW("FFMpegVideoDecoder create thread failed");
        }
    }
    // Initialize avcodec
};

AvcodecVideoDecoder::~AvcodecVideoDecoder() {
    // free av_packet_data_
    while (!av_packet_data_.empty()) {
        std::pair<uint8_t *, int> *packet_data = &av_packet_data_.back();
        av_freep(&packet_data->first);
        av_packet_data_.pop_back();
    }
    //release av_packets
    while (!av_packets_.empty()) {
        av_packet_free(&av_packets_.back());
        av_packets_.pop_back();
    }

    if (dec_context_) {
        avcodec_free_context(&dec_context_);
    }

}

rocDecStatus AvcodecVideoDecoder::InitializeDecoder() {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;
    if (!decoder_) decoder_ = avcodec_find_decoder(RocDecVideoCodec2AVCodec(decoder_create_info_.codec_type));
    if(!decoder_) {
        THROW("rocDecode<FFMpeg>:: Codec not supported by FFMpeg ");
        return ROCDEC_NOT_SUPPORTED;
    }
    if (!dec_context_) {
        dec_context_ = avcodec_alloc_context3(decoder_);        //alloc dec_context_
        if (!dec_context_) {
            THROW("Could not allocate video codec context");
        }
        // set codec to automatically determine how many threads suits best for the decoding job
        dec_context_->thread_count = decoder_create_info_.num_decode_threads;

        if (decoder_->capabilities & AV_CODEC_CAP_FRAME_THREADS)
            dec_context_->thread_type = FF_THREAD_FRAME;
        else if (decoder_->capabilities & AV_CODEC_CAP_SLICE_THREADS)
            dec_context_->thread_type = FF_THREAD_SLICE;
        else
            dec_context_->thread_count = 1;

        // open the codec
        if (avcodec_open2(dec_context_, decoder_, NULL) < 0) {
            THROW("Could not open codec");
        }
        // get the output pixel format from dec_context_
        decoder_pixel_format_ = (dec_context_->pix_fmt == AV_PIX_FMT_NONE) ? AV_PIX_FMT_YUV420P : dec_context_->pix_fmt;
    }
    // allocate av_frame buffer pool for number of surfaces to be in the decoder pool
    // Note: with multi-threading, av_codec needs (dec_context_->delay + max_num_B_frames) number of av_frames. 
    // max_num_B_frames is assumed to be 4 here
    if (dec_frames_.empty()) {
        for (int i = 0; i < (dec_context_->delay + 4); i++) {
            AVFrame *p_frame = av_frame_alloc();
            dec_frames_.push_back(p_frame);
        }
        av_frame_cnt_ = 0;
    }
    // allocate max. packet_q of 4
    if (av_packet_data_.empty()) {
        for (int i = 0; i < 4; i++) {
            uint8_t *pkt_data = static_cast<uint8_t *> (av_malloc(MAX_AV_PACKET_DATA_SIZE));
            av_packet_data_.push_back(std::make_pair(pkt_data, MAX_AV_PACKET_DATA_SIZE));
        }
    }
    // allocate av_packets_ for decoding
    if (av_packets_.empty()) {
        for (int i = 0; i < 4; i++) {
            AVPacket *pkt = av_packet_alloc();
            pkt->data = static_cast<uint8_t *> (av_packet_data_[i].first);
            pkt->size = av_packet_data_[i].second;
            av_packets_.push_back(pkt);
        }
    }

    return rocdec_status;
}

rocDecStatus AvcodecVideoDecoder::SubmitDecode(RocdecPicParamsHost *pPicParams) {
    AVPacket *av_pkt = av_packets_[av_pkt_cnt_];
    std::pair<uint8_t *, int> *packet_data = &av_packet_data_[av_pkt_cnt_];
    if (pPicParams->bitstream_data_len > packet_data->second) {
        void *new_pkt_data = av_realloc(av_pkt->data, (pPicParams->bitstream_data_len + MAX_AV_PACKET_DATA_SIZE));  // add more to avoid frequence reallocation
        if (!new_pkt_data) {
            std::cerr << "ERROR: couldn't allocate packet data" << std::endl;
        }
        packet_data->first   = static_cast<uint8_t *>(new_pkt_data);
        packet_data->second  = (pPicParams->bitstream_data_len + MAX_AV_PACKET_DATA_SIZE);
        av_pkt->data = packet_data->first;
    }
    memcpy(av_pkt->data, pPicParams->bitstream_data, pPicParams->bitstream_data_len);
    av_pkt->size = pPicParams->bitstream_data_len;
    av_pkt->flags = 0;
    av_pkt->pts = last_packet_.pts;

    if (!b_multithreading_) {
        DecodeAvFrame(av_pkt, dec_frames_[av_frame_cnt_]);
        NotifyPictureDisplay();
        if (!pPicParams->bitstream_data_len && !end_of_stream_) {
            AVPacket pkt = {0};
            DecodeAvFrame(&pkt, dec_frames_[av_frame_cnt_]);
            NotifyPictureDisplay();
        }
    } else {
        //push packet into packet q for decoding
        PushPacket(av_pkt);
        if (!pPicParams->bitstream_data_len && !end_of_stream_) {
            AVPacket pkt = {0};
            PushPacket(&pkt);
        }
    }
    av_pkt_cnt_ = (av_pkt_cnt_ + 1) % av_packets_.size();
    if (!av_pkt->data || !av_pkt->size) {
        end_of_stream_ = true;
    }
    if (pPicParams->flags & ROCDEC_PKT_ENDOFSTREAM) {
         // flush last packet and let FFMpeg decode last frames
         NotifyPictureDisplay();
    }

    return ROCDEC_SUCCESS;
}

rocDecStatus AvcodecVideoDecoder::GetDecodeStatus(int pic_idx, RocdecDecodeStatus *decode_status) {
    return ROCDEC_SUCCESS;
}

rocDecStatus AvcodecVideoDecoder::ReconfigureDecoder(RocdecReconfigureDecoderInfo *reconfig_params) {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;
    if (reconfig_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    return rocdec_status;
}

void AvcodecVideoDecoder::DecodeThread()
{
    AVPacket *pkt;
    do {
        pkt = PopPacket();
        DecodeAvFrame(pkt, dec_frames_[av_frame_cnt_]);
        // display frames
        NotifyPictureDisplay();
    } while (!end_of_stream_);
}

int AvcodecVideoDecoder::DecodeAvFrame(AVPacket *av_pkt, AVFrame *p_frame) {
    int status;
    //send packet to av_codec
    status = avcodec_send_packet(dec_context_, av_pkt);
    if (status < 0) {
        std::cout << "Error sending av packet for decoding: status: " << status << std::endl;
    }
    while (status >= 0) {
        status = avcodec_receive_frame(dec_context_, p_frame);
        if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
            end_of_stream_ = (status == AVERROR_EOF);
            return 0;
        }
        else if (status < 0) {
            std::cout << "Error during decoding" << std::endl;
            return 0;
        }
        // for the first frame, initialize OutputsurfaceInfo
        if (p_frame->width != coded_width_ || p_frame->height != coded_height_ || p_frame->format != av_sample_format) {
            coded_width_ = p_frame->width;
            coded_height_ = p_frame->height;
            av_sample_format = p_frame->format;
            NotifyNewSequence(p_frame);
        }

        decoded_pic_cnt_++;
        DecFrameBufferFFMpeg dec_frame = { 0 };
        dec_frame.av_frame_ptr = p_frame;
        dec_frame.pts = p_frame->pts;
        dec_frame.picture_index = av_frame_cnt_;     //picture_index is not used here since it is handled within FFMpeg decoder
        // if (!b_multithreading_) {
        //     disp_frames_q_.push_back(dec_frame);
        //     av_frame_q_.push(p_frame);
        // }
        // else {
        //     PushFrame(p_frame);  // add frame to the frame_q
        //     PushDispFrame(dec_frame)
        // }
        PushDisplayFrame(&dec_frame);
        av_frame_cnt_ = (av_frame_cnt_ + 1) % dec_frames_.size();
        p_frame = dec_frames_[av_frame_cnt_]; //advance for next frame decode
    }
    return 0;
}

rocDecStatus AvcodecVideoDecoder::NotifyNewSequence(AVFrame *p_frame) {
    if (!p_frame)
        return ROCDEC_INVALID_PARAMETER;
    video_format_.codec = decoder_create_info_.codec_type;
    video_format_.frame_rate.numerator = dec_context_->framerate.num;
    video_format_.frame_rate.denominator = dec_context_->framerate.den;
    video_format_.bit_depth_luma_minus8 = dec_context_->bits_per_coded_sample - 8;
    video_format_.bit_depth_chroma_minus8 = dec_context_->bits_per_coded_sample - 8;
    video_format_.progressive_sequence = !p_frame->interlaced_frame;
    video_format_.min_num_decode_surfaces = dec_context_->delay + dec_context_->max_b_frames;
    video_format_.coded_width = p_frame->linesize[0];
    video_format_.coded_height = p_frame->height;
    video_format_.chroma_format = AVPixelFormat2rocDecVideoChromaFormat(dec_context_->pix_fmt);
    video_format_.display_area = { 0, 0, p_frame->width, p_frame->height };
    video_format_.bitrate = 0;
    video_format_.display_aspect_ratio.x = p_frame->sample_aspect_ratio.num;
    video_format_.display_aspect_ratio.y = p_frame->sample_aspect_ratio.den;
    if (pfn_sequece_cb_(decoder_create_info_.user_data, &video_format_) == 0) {
        ERR("Sequence callback function failed.");
        return ROCDEC_RUNTIME_ERROR;
    } else {
        return ROCDEC_SUCCESS;
    }
}

rocDecStatus AvcodecVideoDecoder::SendSeiMsgPayload(AVFrame *p_frame) {
#if 0    
    sei_message_info_params_.sei_message_count = sei_message_count_;
    sei_message_info_params_.sei_message = sei_message_list_.data();
    sei_message_info_params_.sei_data = (void*)sei_payload_buf_;
    sei_message_info_params_.picIdx = curr_pic_info_.dec_buf_idx;

    // callback function with RocdecSeiMessageInfo params filled out
    if (pfn_get_sei_message_cb_) pfn_get_sei_message_cb_(parser_params_.user_data, &sei_message_info_params_);
#endif
    return ROCDEC_NOT_IMPLEMENTED;
}

rocDecStatus AvcodecVideoDecoder::NotifyPictureDisplay() {
    int num_frames_to_display = decoded_pic_cnt_;
    while (num_frames_to_display) {
        auto p_disp_frame = PopDisplayFrame();
        RocdecParserDispInfo dispInfo = {0}; // dispinfo is not used in ffmpeg decoder, so setting it to zero
        dispInfo.picture_index = p_disp_frame->picture_index;
        if (pfn_display_picture_cb_ && decoder_create_info_.user_data) {
            pfn_display_picture_cb_(decoder_create_info_.user_data, &dispInfo);
        }
        num_frames_to_display--;
    };

    return ROCDEC_SUCCESS;
}
