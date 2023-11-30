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

#include "roc_video_dec.h"

RocVideoDecoder::RocVideoDecoder(int device_id, OutputSurfaceMemoryType out_mem_type, rocDecVideoCodec codec,  bool b_low_latency, bool force_zero_latency,
              const Rect *p_crop_rect, bool extract_user_sei_Message, int max_width, int max_height,uint32_t clk_rate) : 
              device_id_{device_id}, out_mem_type_(out_mem_type), codec_id_(codec), b_low_latency_(b_low_latency), 
              b_force_zero_latency_(force_zero_latency), b_extract_sei_message_(extract_user_sei_Message),
              max_width_ (max_width), max_height_(max_height) {

    if (!InitHIP(device_id_)) {
        THROW("Failed to initilize the HIP");
    }
    if (p_crop_rect) crop_rect_ = *p_crop_rect;
    if (b_extract_sei_message_) {
        fp_sei_ = fopen("rocdec_sei_message.txt", "wb");
        curr_sei_message_ptr_ = new RocdecSeiMessageInfo;
        memset(&sei_message_display_q_, 0, sizeof(sei_message_display_q_));
    }
    // create rocdec videoparser
    RocdecParserParams parser_params = {};
    parser_params.CodecType = codec_id_;
    parser_params.ulMaxNumDecodeSurfaces = 1;
    parser_params.ulClockRate = clk_rate;
    parser_params.ulMaxDisplayDelay = b_low_latency ? 0 : 1;
    parser_params.pUserData = this;
    parser_params.pfnSequenceCallback = HandleVideoSequenceProc;
    parser_params.pfnDecodePicture = HandlePictureDecodeProc;
    parser_params.pfnDisplayPicture = b_force_zero_latency_ ? NULL : HandlePictureDisplayProc;
    parser_params.pfnGetSEIMsg = b_extract_sei_message_ ? HandleSEIMessagesProc : NULL;
    ROCDEC_API_CALL(rocDecCreateVideoParser(&rocdec_parser_, &parser_params));
}


RocVideoDecoder::~RocVideoDecoder() {
    if (curr_sei_message_ptr_) {
        delete curr_sei_message_ptr_;
        curr_sei_message_ptr_ = nullptr;
    }

    if (fp_sei_) {
        fclose(fp_sei_);
        fp_sei_ = nullptr;
    }

    if (rocdec_parser_) {
        rocDecDestroyVideoParser(rocdec_parser_);
        rocdec_parser_ = nullptr;
    }

    if (roc_decoder_) {
        rocDecDestroyDecoder(roc_decoder_);
        roc_decoder_ = nullptr;
    }

    if (hip_stream_) {
        hipError_t hip_status = hipSuccess;
        hip_status = hipStreamDestroy(hip_stream_);
        if (hip_status != hipSuccess) {
            std::cout << "ERROR: hipStream_Destroy failed! (" << hip_status << ")" << std::endl;
        }
    }
}

static const char * GetVideoCodecString(rocDecVideoCodec e_codec) {
    static struct {
        rocDecVideoCodec e_codec;
        const char *name;
    } aCodecName [] = {
        { rocDecVideoCodec_MPEG1,     "MPEG-1"       },
        { rocDecVideoCodec_MPEG2,     "MPEG-2"       },
        { rocDecVideoCodec_MPEG4,     "MPEG-4 (ASP)" },
        { rocDecVideoCodec_H264,      "AVC/H.264"    },
        { rocDecVideoCodec_HEVC,      "H.265/HEVC"   },
        { rocDecVideoCodec_AV1,       "AV1"          },
        { rocDecVideoCodec_VP8,       "VP8"          },
        { rocDecVideoCodec_VP9,       "VP9"          },
        { rocDecVideoCodec_JPEG,      "M-JPEG"       },
        { rocDecVideoCodec_NumCodecs, "Invalid"      },
    };

    if (e_codec >= 0 && e_codec <= rocDecVideoCodec_NumCodecs) {
        return aCodecName[e_codec].name;
    }
    for (int i = rocDecVideoCodec_NumCodecs + 1; i < sizeof(aCodecName) / sizeof(aCodecName[0]); i++) {
        if (e_codec == aCodecName[i].e_codec) {
            return aCodecName[e_codec].name;
        }
    }
    return "Unknown";
}



/**
 * @brief function to return the name from codec_id
 * 
 * @param codec_id 
 * @return const char* 
 */
const char *RocVideoDecoder::GetCodecFmtName(rocDecVideoCodec codec_id)
{
    return GetVideoCodecString(codec_id);
}

static const char * GetSurfaceFormatString(rocDecVideoSurfaceFormat surface_format_id) {
    static struct {
        rocDecVideoSurfaceFormat surf_fmt;
        const char *name;
    } SurfName [] = {
        { rocDecVideoSurfaceFormat_NV12,                    "NV12" },
        { rocDecVideoSurfaceFormat_P016,                    "P016" },
        { rocDecVideoSurfaceFormat_YUV444,                "YUV444" },
        { rocDecVideoSurfaceFormat_YUV444_16Bit,    "YUV444_16Bit" },
    };

    if (surface_format_id >= rocDecVideoSurfaceFormat_NV12 && surface_format_id <= rocDecVideoSurfaceFormat_YUV444_16Bit)
        return SurfName[surface_format_id].name;
    else
        return "Unknown";
}

/**
 * @brief function to return the name from surface_format_id
 * 
 * @param surface_format_id - enum for surface format
 * @return const char* 
 */
const char *RocVideoDecoder::GetSurfaceFmtName(rocDecVideoSurfaceFormat surface_format_id)
{
    return GetSurfaceFormatString(surface_format_id);
}

static const char * GetVideoChromaFormatName(rocDecVideoChromaFormat e_chroma_format) {
    static struct {
        rocDecVideoChromaFormat chroma_fmt;
        const char *name;
    } ChromaFormatName[] = {
        { rocDecVideoChromaFormat_Monochrome, "YUV 400 (Monochrome)" },
        { rocDecVideoChromaFormat_420,        "YUV 420"              },
        { rocDecVideoChromaFormat_422,        "YUV 422"              },
        { rocDecVideoChromaFormat_444,        "YUV 444"              },
    };

    if (e_chroma_format >= 0 && e_chroma_format <= rocDecVideoChromaFormat_444) {
        return ChromaFormatName[e_chroma_format].name;
    }
    return "Unknown";
}

static float GetChromaHeightFactor(rocDecVideoSurfaceFormat surface_format) {
    float factor = 0.5;
    switch (surface_format) {
    case rocDecVideoSurfaceFormat_NV12:
    case rocDecVideoSurfaceFormat_P016:
        factor = 0.5;
        break;
    case rocDecVideoSurfaceFormat_YUV444:
    case rocDecVideoSurfaceFormat_YUV444_16Bit:
        factor = 1.0;
        break;
    }

    return factor;
}

static int GetChromaPlaneCount(rocDecVideoSurfaceFormat surface_format) {
    int num_planes = 1;
    switch (surface_format) {
    case rocDecVideoSurfaceFormat_NV12:
    case rocDecVideoSurfaceFormat_P016:
        num_planes = 1;
        break;
    case rocDecVideoSurfaceFormat_YUV444:
    case rocDecVideoSurfaceFormat_YUV444_16Bit:
        num_planes = 2;
        break;
    }

    return num_planes;
}

static void GetSurfaceStrideInternal(rocDecVideoSurfaceFormat surface_format, uint32_t width, uint32_t height, uint32_t *pitch, uint32_t *vstride) {

    switch (surface_format) {
    case rocDecVideoSurfaceFormat_NV12:
        *pitch = align(width, 256);
        *vstride = align(height, 16);
        break;
    case rocDecVideoSurfaceFormat_P016:
        *pitch = align(width, 128) * 2;
        *vstride = align(height, 16);
        break;
    case rocDecVideoSurfaceFormat_YUV444:
        *pitch = align(width, 256);
        *vstride = align(height, 16);
        break;
    case rocDecVideoSurfaceFormat_YUV444_16Bit:
        *pitch = align(width, 128) * 2;
        *vstride = align(height, 16);
        break;
    }
    return;
}

/* Return value from HandleVideoSequence() are interpreted as   :
*  0: fail, 1: succeeded, > 1: override dpb size of parser (set by CUVIDPARSERPARAMS::ulMaxNumDecodeSurfaces while creating parser)
*/
int RocVideoDecoder::HandleVideoSequence(RocdecVideoFormat *p_video_format) {
    //START_TIMER
    input_video_info_str_.str("");
    input_video_info_str_.clear();
    input_video_info_str_ << "Input Video Information" << std::endl
        << "\tCodec        : " << GetCodecFmtName(p_video_format->codec) << std::endl
        << "\tFrame rate   : " << p_video_format->frame_rate.numerator << "/" << p_video_format->frame_rate.denominator
            << " = " << 1.0 * p_video_format->frame_rate.numerator / p_video_format->frame_rate.denominator << " fps" << std::endl
        << "\tSequence     : " << (p_video_format->progressive_sequence ? "Progressive" : "Interlaced") << std::endl
        << "\tCoded size   : [" << p_video_format->coded_width << ", " << p_video_format->coded_height << "]" << std::endl
        << "\tDisplay area : [" << p_video_format->display_area.left << ", " << p_video_format->display_area.top << ", "
            << p_video_format->display_area.right << ", " << p_video_format->display_area.bottom << "]" << std::endl
        << "\tChroma       : " << GetVideoChromaFormatName(p_video_format->chroma_format) << std::endl
        << "\tBit depth    : " << p_video_format->bit_depth_luma_minus8 + 8
    ;
    input_video_info_str_ << std::endl;

    int nDecodeSurface = p_video_format->min_num_decode_surfaces;

    RocdecDecodeCaps decode_caps;
    memset(&decode_caps, 0, sizeof(decode_caps));
    decode_caps.eCodecType = p_video_format->codec;
    decode_caps.eChromaFormat = p_video_format->chroma_format;
    decode_caps.nBitDepthMinus8 = p_video_format->bit_depth_luma_minus8;

    ROCDEC_API_CALL(rocDecGetDecoderCaps(&decode_caps));

    if(!decode_caps.bIsSupported) {
        ROCDEC_THROW("Rocdec:: Codec not supported on this GPU: ", ROCDEC_NOT_SUPPORTED);
        return 0;
    }

    if ((p_video_format->coded_width > decode_caps.nMaxWidth) ||
        (p_video_format->coded_height > decode_caps.nMaxHeight)) {

        std::ostringstream errorString;
        errorString << std::endl
                    << "Resolution          : " << p_video_format->coded_width << "x" << p_video_format->coded_height << std::endl
                    << "Max Supported (wxh) : " << decode_caps.nMaxWidth << "x" << decode_caps.nMaxHeight << std::endl
                    << "Resolution not supported on this GPU ";

        const std::string cErr = errorString.str();
        ROCDEC_THROW(cErr, ROCDEC_NOT_SUPPORTED);
        return 0;
    }

    if (width_ && height_ && chroma_height_) {

        // rocdecCreateDecoder() has been called before, and now there's possible config change
        // todo:: support reconfigure
        //return ReconfigureDecoder(p_video_format);
    }

    // e_codec has been set in the constructor (for parser). Here it's set again for potential correction
    codec_id_ = p_video_format->codec;
    video_chroma_format_ = p_video_format->chroma_format;
    bitdepth_minus_8_ = p_video_format->bit_depth_luma_minus8;
    byte_per_pixel_ = bitdepth_minus_8_ > 0 ? 2 : 1;

    // Set the output surface format same as chroma format
    if (video_chroma_format_ == rocDecVideoChromaFormat_420 || rocDecVideoChromaFormat_Monochrome)
        video_surface_format_ = p_video_format->bit_depth_luma_minus8 ? rocDecVideoSurfaceFormat_P016 : rocDecVideoSurfaceFormat_NV12;
    else if (video_chroma_format_ == rocDecVideoChromaFormat_444)
        video_surface_format_ = p_video_format->bit_depth_luma_minus8 ? rocDecVideoSurfaceFormat_YUV444_16Bit : rocDecVideoSurfaceFormat_YUV444;
    else if (video_chroma_format_ == rocDecVideoChromaFormat_422)
        video_surface_format_ = rocDecVideoSurfaceFormat_NV12;

    // Check if output format supported. If not, check falback options
    if (!(decode_caps.nOutputFormatMask & (1 << video_surface_format_))){
        if (decode_caps.nOutputFormatMask & (1 << rocDecVideoSurfaceFormat_NV12))
            video_surface_format_ = rocDecVideoSurfaceFormat_NV12;
        else if (decode_caps.nOutputFormatMask & (1 << rocDecVideoSurfaceFormat_P016))
            video_surface_format_ = rocDecVideoSurfaceFormat_P016;
        else if (decode_caps.nOutputFormatMask & (1 << rocDecVideoSurfaceFormat_YUV444))
            video_surface_format_ = rocDecVideoSurfaceFormat_YUV444;
        else if (decode_caps.nOutputFormatMask & (1 << rocDecVideoSurfaceFormat_YUV444_16Bit))
            video_surface_format_ = rocDecVideoSurfaceFormat_YUV444_16Bit;
        else 
            ROCDEC_THROW("No supported output format found", ROCDEC_NOT_SUPPORTED);
    }
    video_format_ = *p_video_format;

    RocDecoderCreateInfo videoDecodeCreateInfo = { 0 };
    videoDecodeCreateInfo.deviceid = device_id_;
    videoDecodeCreateInfo.CodecType = p_video_format->codec;
    videoDecodeCreateInfo.ChromaFormat = p_video_format->chroma_format;
    videoDecodeCreateInfo.OutputFormat = video_surface_format_;
    videoDecodeCreateInfo.bitDepthMinus8 = p_video_format->bit_depth_luma_minus8;
    videoDecodeCreateInfo.ulNumOutputSurfaces = 2;
    videoDecodeCreateInfo.ulNumDecodeSurfaces = nDecodeSurface;
    videoDecodeCreateInfo.ulWidth = p_video_format->coded_width;
    videoDecodeCreateInfo.ulHeight = p_video_format->coded_height;
    // AV1 has max width/height of sequence in sequence header
    if (p_video_format->codec == rocDecVideoCodec_AV1 && p_video_format->seqhdr_data_length > 0) {
        // dont overwrite if it is already set from cmdline or reconfig.txt
        if (!(max_width_ > p_video_format->coded_width || max_height_ > p_video_format->coded_height)) {
            RocdecVideoFormatEx *vidFormatEx = (RocdecVideoFormatEx *)p_video_format;
            max_width_ = vidFormatEx->max_width;
            max_height_ = vidFormatEx->max_height;
        }
    }
    if (max_width_ < (int)p_video_format->coded_width)
        max_width_ = p_video_format->coded_width;
    if (max_height_ < (int)p_video_format->coded_height)
        max_height_ = p_video_format->coded_height;

    videoDecodeCreateInfo.ulMaxWidth = max_width_;
    videoDecodeCreateInfo.ulMaxHeight = max_height_;

    if (!(crop_rect_.r && crop_rect_.b)) {
        width_ = p_video_format->display_area.right - p_video_format->display_area.left;
        height_ = p_video_format->display_area.bottom - p_video_format->display_area.top;
        videoDecodeCreateInfo.ulTargetWidth = width_;
        videoDecodeCreateInfo.ulTargetHeight = height_;
    } else {
        videoDecodeCreateInfo.display_area.left = crop_rect_.l;
        videoDecodeCreateInfo.display_area.top = crop_rect_.t;
        videoDecodeCreateInfo.display_area.right = crop_rect_.r;
        videoDecodeCreateInfo.display_area.bottom = crop_rect_.b;
        width_ = crop_rect_.r - crop_rect_.l;
        height_ = crop_rect_.b - crop_rect_.t;
        videoDecodeCreateInfo.ulTargetWidth = (width_ + 1) & ~1;
        videoDecodeCreateInfo.ulTargetHeight = (height_ + 1) & ~1;
    }

    chroma_height_ = (int)(ceil(height_ * GetChromaHeightFactor(video_surface_format_)));
    num_chroma_planes_ = GetChromaPlaneCount(video_surface_format_);
    if (p_video_format->chroma_format == rocDecVideoChromaFormat_Monochrome) num_chroma_planes_ = 0;
    if (out_mem_type_ == OUT_SURFACE_MEM_DEV_INTERNAL)
        GetSurfaceStrideInternal(video_surface_format_, p_video_format->coded_width, p_video_format->coded_height, &surface_stride_, &surface_vstride_);
    else {
        surface_stride_ = videoDecodeCreateInfo.ulTargetWidth * byte_per_pixel_;    // todo:: check if we need pitched memory for faster copy
    }
    chroma_vstride_ = (int)(ceil(surface_vstride_ * GetChromaHeightFactor(video_surface_format_)));
    // fill output_surface_info_
    output_surface_info_.output_width = width_;
    output_surface_info_.output_height = height_;
    output_surface_info_.output_pitch  = surface_stride_;
    output_surface_info_.output_vstride = (out_mem_type_ == OUT_SURFACE_MEM_DEV_INTERNAL) ? surface_vstride_ : videoDecodeCreateInfo.ulTargetHeight;
    output_surface_info_.bit_depth = bitdepth_minus_8_ + 8;
    output_surface_info_.bytes_per_pixel = byte_per_pixel_;
    output_surface_info_.surface_format = video_surface_format_;
    output_surface_info_.num_chroma_planes = num_chroma_planes_;
    if (out_mem_type_ == OUT_SURFACE_MEM_DEV_INTERNAL) {
        output_surface_info_.output_surface_size_in_bytes = surface_stride_ * (surface_vstride_ + (chroma_vstride_ * num_chroma_planes_));
        output_surface_info_.mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;
    } else if (out_mem_type_ == OUT_SURFACE_MEM_DEV_COPIED) {
        output_surface_info_.output_surface_size_in_bytes = GetFrameSizePitched();
        output_surface_info_.mem_type = OUT_SURFACE_MEM_DEV_COPIED;
    } else {
        output_surface_info_.output_surface_size_in_bytes = GetFrameSize();
        output_surface_info_.mem_type = OUT_SURFACE_MEM_HOST_COPIED;
    }

    disp_rect_.b = videoDecodeCreateInfo.display_area.bottom;
    disp_rect_.t = videoDecodeCreateInfo.display_area.top;
    disp_rect_.l = videoDecodeCreateInfo.display_area.left;
    disp_rect_.r = videoDecodeCreateInfo.display_area.right;

    input_video_info_str_ << "Video Decoding Params:" << std::endl
        << "\tNum Surfaces : " << videoDecodeCreateInfo.ulNumDecodeSurfaces << std::endl
        << "\tCrop         : [" << videoDecodeCreateInfo.display_area.left << ", " << videoDecodeCreateInfo.display_area.top << ", "
        << videoDecodeCreateInfo.display_area.right << ", " << videoDecodeCreateInfo.display_area.bottom << "]" << std::endl
        << "\tResize       : " << videoDecodeCreateInfo.ulTargetWidth << "x" << videoDecodeCreateInfo.ulTargetHeight << std::endl
    ;
    input_video_info_str_ << std::endl;
    std::cout << input_video_info_str_.str();

    ROCDEC_API_CALL(rocDecCreateDecoder(&roc_decoder_, &videoDecodeCreateInfo));
    return nDecodeSurface;
}


int RocVideoDecoder::ReconfigureDecoder(RocdecVideoFormat *p_video_format) {
    ROCDEC_THROW("ReconfigureDecoder is not supported in this version: ", ROCDEC_NOT_SUPPORTED);
    return ROCDEC_NOT_SUPPORTED;
}

/**
 * @brief 
 * 
 * @param pPicParams 
 * @return int 1: success 0: fail
 */
int RocVideoDecoder::HandlePictureDecode(RocdecPicParams *pPicParams) {
    if (!roc_decoder_) {
        THROW("RocDecoder not initialized: failed with ErrCode: " +  TOSTR(ROCDEC_NOT_INITIALIZED));
    }
    pic_num_in_dec_order_[pPicParams->CurrPicIdx] = decode_poc_++;
    ROCDEC_API_CALL(rocDecDecodeFrame(roc_decoder_, pPicParams));
    if (b_force_zero_latency_ && ((!pPicParams->field_pic_flag) || (pPicParams->second_field))) {
        RocdecParserDispInfo disp_info;
        memset(&disp_info, 0, sizeof(disp_info));
        disp_info.picture_index = pPicParams->CurrPicIdx;
        disp_info.progressive_frame = !pPicParams->field_pic_flag;
        disp_info.top_field_first = pPicParams->bottom_field_flag ^ 1;
        HandlePictureDisplay(&disp_info);
    }
    return 1;
}

/**
 * @brief function to handle display picture
 * 
 * @param pDispInfo 
 * @return int 0:fail 1: success
 */
int RocVideoDecoder::HandlePictureDisplay(RocdecParserDispInfo *pDispInfo) {
    RocdecProcParams video_proc_params = {};
    video_proc_params.progressive_frame = pDispInfo->progressive_frame;
    video_proc_params.top_field_first = pDispInfo->top_field_first;
    video_proc_params.output_hipstream = hip_stream_;

    if (b_extract_sei_message_) {
        if (sei_message_display_q_[pDispInfo->picture_index].pSEIData) {
            // Write SEI Message
            uint8_t *sei_buffer = (uint8_t *)(sei_message_display_q_[pDispInfo->picture_index].pSEIData);
            uint32_t sei_num_messages = sei_message_display_q_[pDispInfo->picture_index].sei_message_count;
            RocdecSeiMessage *sei_message = sei_message_display_q_[pDispInfo->picture_index].pSEIMessage;
            if (fp_sei_) {
                for (uint32_t i = 0; i < sei_num_messages; i++) {
                    if (codec_id_ == rocDecVideoCodec_H264 || rocDecVideoCodec_HEVC) {
                        switch (sei_message[i].sei_message_type) {
                            case SEI_TYPE_TIME_CODE: {
                                //todo:: check if we need to write timecode
                            }
                            break;
                            case SEI_TYPE_USER_DATA_UNREGISTERED: {
                                fwrite(sei_buffer, sei_message[i].sei_message_size, 1, fp_sei_);
                            }
                            break;
                        }
                    }
                    if (codec_id_ == rocDecVideoCodec_AV1) {
                        fwrite(sei_buffer, sei_message[i].sei_message_size, 1, fp_sei_);
                    }    
                    sei_buffer += sei_message[i].sei_message_size;
                }
            }
            free(sei_message_display_q_[pDispInfo->picture_index].pSEIData);
            sei_message_display_q_[pDispInfo->picture_index].pSEIData = NULL; // to avoid double free
            free(sei_message_display_q_[pDispInfo->picture_index].pSEIMessage);
            sei_message_display_q_[pDispInfo->picture_index].pSEIMessage = NULL; // to avoid double free
        }
    }

    void * src_dev_ptr[3] = { 0 };
    uint32_t src_pitch[3] = { 0 };
    ROCDEC_API_CALL(rocDecMapVideoFrame(roc_decoder_, pDispInfo->picture_index, src_dev_ptr, src_pitch, &video_proc_params));
    RocdecDecodeStatus dec_status;
    memset(&dec_status, 0, sizeof(dec_status));
    rocDecStatus result = rocDecGetDecodeStatus(roc_decoder_, pDispInfo->picture_index, &dec_status);
    if (result == ROCDEC_SUCCESS && (dec_status.decodeStatus == rocDecodeStatus_Error || dec_status.decodeStatus == rocDecodeStatus_Error_Concealed)) {
        std::cerr << "Decode Error occurred for picture: " << pic_num_in_dec_order_[pDispInfo->picture_index] << std::endl;
    }
    if (out_mem_type_ == OUT_SURFACE_MEM_DEV_INTERNAL) {
        DecFrameBuffer dec_frame = { 0 };
        dec_frame.frame_ptr = (uint8_t *)(src_dev_ptr[0]);
        dec_frame.pts = pDispInfo->pts;
        dec_frame.picture_index = pDispInfo->picture_index;
        std::lock_guard<std::mutex> lock(mtx_vp_frame_);
        vp_frames_q_.push(dec_frame);
        decoded_frame_cnt_++;
    } else {
        // copy the decoded surface info device or host
        uint8_t *p_dec_frame = nullptr;
        {
            std::lock_guard<std::mutex> lock(mtx_vp_frame_);
            // if not enough frames in stock, allocate
            if ((unsigned)++decoded_frame_cnt_ > vp_frames_.size()) {
                num_alloced_frames_++;
                DecFrameBuffer dec_frame = { 0 };
                if (out_mem_type_ == OUT_SURFACE_MEM_DEV_COPIED) {
                    // allocate device memory
                    HIP_API_CALL(hipMalloc((void **)&dec_frame.frame_ptr, GetFrameSizePitched()));
                } else {
                    dec_frame.frame_ptr = new uint8_t[GetFrameSize()];
                }
                dec_frame.pts = pDispInfo->pts;
                dec_frame.picture_index = pDispInfo->picture_index;
                vp_frames_.push_back(dec_frame);
            }
            p_dec_frame = vp_frames_[decoded_frame_cnt_ - 1].frame_ptr;
        }
        // Copy luma data
        int dst_pitch = surface_stride_;
        if (out_mem_type_ == OUT_SURFACE_MEM_DEV_COPIED) {
            if (src_pitch[0] == dst_pitch) {
                int luma_size = src_pitch[0] * height_;
                HIP_API_CALL(hipMemcpyDtoDAsync(p_dec_frame, src_dev_ptr[0], luma_size, hip_stream_));
            } else {
                // use 2d copy to copy an ROI
                HIP_API_CALL(hipMemcpy2DAsync(p_dec_frame, dst_pitch, src_dev_ptr[0], src_pitch[0], width_ * byte_per_pixel_, height_, hipMemcpyDeviceToDevice, hip_stream_));
            }
        } else
            HIP_API_CALL(hipMemcpy2DAsync(p_dec_frame, width_ * byte_per_pixel_, src_dev_ptr[0], src_pitch[0], width_ * byte_per_pixel_, height_, hipMemcpyDeviceToHost, hip_stream_));

        // Copy chroma plane ( )
        // rocDec output gives pointer to luma and chroma pointers seperated for the decoded frame
        uint8_t *p_frame_uv = p_dec_frame + dst_pitch * height_;
        if (out_mem_type_ == OUT_SURFACE_MEM_DEV_COPIED) {
            if (src_pitch[1] == dst_pitch) {
                int chroma_size = chroma_height_ * dst_pitch;
                HIP_API_CALL(hipMemcpyDtoDAsync(p_frame_uv, src_dev_ptr[1], chroma_size, hip_stream_));
            } else {
                // use 2d copy to copy an ROI
                HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[1], src_pitch[1], width_ * byte_per_pixel_, chroma_height_, hipMemcpyDeviceToDevice, hip_stream_));
            }
        } else
            HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[1], src_pitch[1], width_ * byte_per_pixel_, chroma_height_, hipMemcpyDeviceToHost, hip_stream_));

        if (num_chroma_planes_ == 2) {
            uint8_t *p_frame_uv = p_dec_frame + dst_pitch * (height_ + chroma_height_);
            if (out_mem_type_ == OUT_SURFACE_MEM_DEV_COPIED) {
                if (src_pitch[2] == dst_pitch) {
                    int chroma_size = chroma_height_ * dst_pitch;
                    HIP_API_CALL(hipMemcpyDtoDAsync(p_frame_uv, src_dev_ptr[2], chroma_size, hip_stream_));
                } else {
                    // use 2d copy to copy an ROI
                    HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[2], src_pitch[2], width_*byte_per_pixel_, chroma_height_, hipMemcpyDeviceToDevice, hip_stream_));
                }
            } else
                HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[2], src_pitch[2], width_*byte_per_pixel_, chroma_height_, hipMemcpyDeviceToHost, hip_stream_));
        }

        HIP_API_CALL(hipStreamSynchronize(hip_stream_));
        ROCDEC_API_CALL(rocDecUnMapVideoFrame(roc_decoder_, pDispInfo->picture_index));
    }

    return 1;
}

int RocVideoDecoder::GetSEIMessage(RocdecSeiMessageInfo *pSEIMessageInfo) {
    uint32_t sei_num_mesages = pSEIMessageInfo->sei_message_count;
    if (sei_num_mesages) {
      RocdecSeiMessage *p_sei_msg_info = pSEIMessageInfo->pSEIMessage;
      size_t total_SEI_buff_size = 0;
      if ((pSEIMessageInfo->picIdx < 0) || (pSEIMessageInfo->picIdx >= MAX_FRAME_NUM)) {
          ERR("Invalid picture index for SEI message: " + TOSTR(pSEIMessageInfo->picIdx));
          return 0;
      }
      for (uint32_t i = 0; i < sei_num_mesages; i++) {
          total_SEI_buff_size += p_sei_msg_info[i].sei_message_size;
      }
      if (!curr_sei_message_ptr_) {
          ERR("Out of Memory, Allocation failed for m_pCurrSEIMessage");
          return 0;
      }
      curr_sei_message_ptr_->pSEIData = malloc(total_SEI_buff_size);
      if (!curr_sei_message_ptr_->pSEIData) {
          ERR("Out of Memory, Allocation failed for SEI Buffer");
          return 0;
      }
      memcpy(curr_sei_message_ptr_->pSEIData, pSEIMessageInfo->pSEIData, total_SEI_buff_size);
      curr_sei_message_ptr_->pSEIMessage = (RocdecSeiMessage *)malloc(sizeof(RocdecSeiMessage) * sei_num_mesages);
      if (!curr_sei_message_ptr_->pSEIMessage) {
          free(curr_sei_message_ptr_->pSEIData);
          curr_sei_message_ptr_->pSEIData = NULL;
          return 0;
      }
      memcpy(curr_sei_message_ptr_->pSEIMessage, pSEIMessageInfo->pSEIMessage, sizeof(RocdecSeiMessage) * sei_num_mesages);
      curr_sei_message_ptr_->sei_message_count = pSEIMessageInfo->sei_message_count;
      sei_message_display_q_[pSEIMessageInfo->picIdx] = *curr_sei_message_ptr_;
    }
    return 1;
}


int RocVideoDecoder::DecodeFrame(const uint8_t *data, size_t size, int pkt_flags, int64_t pts) {
    decoded_frame_cnt_ = 0, decoded_frame_cnt_ret_ = 0;
    RocdecSourceDataPacket packet = { 0 };
    packet.payload = data;
    packet.payload_size = size;
    packet.flags = pkt_flags | ROCDEC_PKT_TIMESTAMP;
    packet.pts = pts;
    if (!data || size == 0) {
        packet.flags |= ROCDEC_PKT_ENDOFSTREAM;
    }
    ROCDEC_API_CALL(rocDecParseVideoData(rocdec_parser_, &packet));

    return decoded_frame_cnt_;
}

uint8_t* RocVideoDecoder::GetFrame(int64_t *pts) {
    if (decoded_frame_cnt_ > 0) {
        std::lock_guard<std::mutex> lock(mtx_vp_frame_);
        decoded_frame_cnt_--;
        if (out_mem_type_ == OUT_SURFACE_MEM_DEV_INTERNAL && !vp_frames_q_.empty()) {
            DecFrameBuffer *fb = &vp_frames_q_.front();
            if (pts) *pts = fb->pts;
            return fb->frame_ptr;
        } else {
            if (pts) *pts = vp_frames_[decoded_frame_cnt_ret_].pts;
            return vp_frames_[decoded_frame_cnt_ret_++].frame_ptr;
        }
    }
    return nullptr;
}

/**
 * @brief function to release frame after use by the application: Only used with "OUT_SURFACE_MEM_DEV_INTERNAL"
 * 
 * @param pTimestamp - timestamp of the frame to be released (unmapped)
 * @return true      - success
 * @return false     - falied
 */

bool RocVideoDecoder::ReleaseFrame(int64_t pTimestamp) {
    if (out_mem_type_ != OUT_SURFACE_MEM_DEV_INTERNAL)
        return true;            // nothing to do
    // only needed when using internal mapped buffer
    if (!vp_frames_q_.empty()) {
        std::lock_guard<std::mutex> lock(mtx_vp_frame_);
        DecFrameBuffer *fb = &vp_frames_q_.front();
        void *mapped_frame_ptr = fb->frame_ptr;

        if (pTimestamp != fb->pts) {
            std::cerr << "Decoded Frame is released out of order" << std::endl;
            return false;
        }
        ROCDEC_API_CALL(rocDecUnMapVideoFrame(roc_decoder_, fb->picture_index));
        // pop decoded frame
        vp_frames_q_.pop();
    }
    return true;
}


void RocVideoDecoder::SaveFrameToFile(std::string output_file_name, void *surf_mem, OutputSurfaceInfo *surf_info) {
    uint8_t *hst_ptr = nullptr;
    uint64_t output_image_size = surf_info->output_surface_size_in_bytes;
    if (surf_info->mem_type == OUT_SURFACE_MEM_DEV_INTERNAL || surf_info->mem_type == OUT_SURFACE_MEM_DEV_COPIED) {
        if (hst_ptr == nullptr) {
            hst_ptr = new uint8_t [output_image_size];
        }
        hipError_t hip_status = hipSuccess;
        hip_status = hipMemcpyDtoH((void *)hst_ptr, surf_mem, output_image_size);
        if (hip_status != hipSuccess) {
            std::cout << "ERROR: hipMemcpyDtoH failed! (" << hip_status << ")" << std::endl;
            delete [] hst_ptr;
            return;
        }
    } else
        hst_ptr = (uint8_t *)surf_mem;


    uint8_t *tmp_hst_ptr = hst_ptr;
    if (fp_out_ == nullptr) {
        fp_out_ = fopen(output_file_name.c_str(), "wb");
    }
    if (fp_out_) {
        int img_width = surf_info->output_width;
        int img_height = surf_info->output_height;
        int output_stride =  surf_info->output_pitch;
        if (img_width * surf_info->bytes_per_pixel == output_stride && img_height == surf_info->output_vstride) {
            fwrite(hst_ptr, 1, output_image_size, fp_out_);
        } else {
            uint32_t width = surf_info->output_width;
            if (surf_info->bit_depth == 8) {
                for (int i = 0; i < surf_info->output_height; i++) {
                    fwrite(tmp_hst_ptr, 1, width, fp_out_);
                    tmp_hst_ptr += output_stride;
                }
                // dump chroma
                uint8_t *uv_hst_ptr = hst_ptr + output_stride * surf_info->output_vstride;
                for (int i = 0; i < chroma_height_; i++) {
                    fwrite(uv_hst_ptr, 1, width, fp_out_);
                    uv_hst_ptr += output_stride;
                }
                if (num_chroma_planes_ == 2) {
                    uint8_t *v_hst_ptr = hst_ptr + output_stride * (surf_info->output_vstride + chroma_vstride_);
                    for (int i = 0; i < chroma_height_; i++) {
                        fwrite(uv_hst_ptr, 1, width, fp_out_);
                        v_hst_ptr += output_stride;
                    }
                }

            } else if (surf_info->bit_depth > 8 &&  surf_info->bit_depth <= 16 ) {
                for (int i = 0; i < img_height; i++) {
                    fwrite(tmp_hst_ptr, 1, width * surf_info->bytes_per_pixel, fp_out_);
                    tmp_hst_ptr += output_stride;
                }
                // dump chroma
                uint8_t *uv_hst_ptr = hst_ptr + output_stride * surf_info->output_vstride;
                for (int i = 0; i < chroma_height_; i++) {
                    fwrite(uv_hst_ptr, 1, width * surf_info->bytes_per_pixel, fp_out_);
                    uv_hst_ptr += output_stride;
                }
                if (num_chroma_planes_ == 2) {
                    uint8_t *v_hst_ptr = hst_ptr + output_stride * (surf_info->output_vstride + chroma_vstride_);
                    for (int i = 0; i < chroma_height_; i++) {
                        fwrite(uv_hst_ptr, 1, width, fp_out_);
                        v_hst_ptr += output_stride;
                    }
                }
            }
        }
    }

    if (hst_ptr && (surf_info->mem_type != OUT_SURFACE_MEM_HOST_COPIED)) {
        delete [] hst_ptr;
    }
}

void RocVideoDecoder::InitMd5() {
    md5_ctx_ = av_md5_alloc();
    av_md5_init(md5_ctx_);
}

void RocVideoDecoder::UpdateMd5ForFrame(void *surf_mem, OutputSurfaceInfo *surf_info) {
    int i;
    uint8_t *hst_ptr = nullptr;
    uint64_t output_image_size = surf_info->output_surface_size_in_bytes;
    if (surf_info->mem_type == OUT_SURFACE_MEM_DEV_INTERNAL || surf_info->mem_type == OUT_SURFACE_MEM_DEV_COPIED) {
        if (hst_ptr == nullptr) {
            hst_ptr = new uint8_t [output_image_size];
        }
        hipError_t hip_status = hipSuccess;
        hip_status = hipMemcpyDtoH((void *)hst_ptr, surf_mem, output_image_size);
        if (hip_status != hipSuccess) {
            std::cout << "ERROR: hipMemcpyDtoH failed! (" << hip_status << ")" << std::endl;
            delete [] hst_ptr;
            return;
        }
    } else
        hst_ptr = (uint8_t *)surf_mem;

    // Need to covert interleaved planar to stacked planar, assuming 4:2:0 chroma sampling.
    uint8_t *stacked_ptr = new uint8_t [output_image_size];

    uint8_t *tmp_hst_ptr = hst_ptr;
    uint8_t *tmp_stacked_ptr = stacked_ptr;
    int img_width = surf_info->output_width;
    int img_height = surf_info->output_height;
    int output_stride =  surf_info->output_pitch;
    // Luma
    if (img_width * surf_info->bytes_per_pixel == output_stride && img_height == surf_info->output_vstride) {
        memcpy(stacked_ptr, hst_ptr, img_width * surf_info->bytes_per_pixel * img_height);
    } else {
        for (i = 0; i < img_height; i++) {
            memcpy(tmp_stacked_ptr, tmp_hst_ptr, img_width * surf_info->bytes_per_pixel);
            tmp_hst_ptr += output_stride;
            tmp_stacked_ptr += img_width * surf_info->bytes_per_pixel;
        }
    }
    // Chroma
    int img_width_chroma = img_width >> 1;
    tmp_hst_ptr = hst_ptr + output_stride * surf_info->output_vstride;
    tmp_stacked_ptr = stacked_ptr + img_width * surf_info->bytes_per_pixel * img_height; // Cb
    uint8_t *tmp_stacked_ptr_v = tmp_stacked_ptr + img_width_chroma * surf_info->bytes_per_pixel * chroma_height_; // Cr
    for (i = 0; i < chroma_height_; i++) {
        for ( int j = 0; j < img_width_chroma; j++) {
            uint8_t *src_ptr, *dst_ptr;
            // Cb
            src_ptr = &tmp_hst_ptr[j * surf_info->bytes_per_pixel * 2];
            dst_ptr = &tmp_stacked_ptr[j * surf_info->bytes_per_pixel];
            memcpy(dst_ptr, src_ptr, surf_info->bytes_per_pixel);
            // Cr
            src_ptr += surf_info->bytes_per_pixel;
            dst_ptr = &tmp_stacked_ptr_v[j * surf_info->bytes_per_pixel];
            memcpy(dst_ptr, src_ptr, surf_info->bytes_per_pixel);
        }
        tmp_hst_ptr += output_stride;
        tmp_stacked_ptr += img_width_chroma * surf_info->bytes_per_pixel;
        tmp_stacked_ptr_v += img_width_chroma * surf_info->bytes_per_pixel;
    }

    int img_size = img_width * surf_info->bytes_per_pixel * (img_height + chroma_height_);
    av_md5_update(md5_ctx_, stacked_ptr, img_size);

    if (hst_ptr && (surf_info->mem_type != OUT_SURFACE_MEM_HOST_COPIED)) {
        delete [] hst_ptr;
    }
    delete [] stacked_ptr;
}

void RocVideoDecoder::FinalizeMd5(uint8_t **digest) {
    av_md5_final(md5_ctx_, md5_digest_);
    av_freep(&md5_ctx_);
    *digest = md5_digest_;
}

void RocVideoDecoder::GetDeviceinfo(std::string &device_name, std::string &gcn_arch_name, int &pci_bus_id, int &pci_domain_id, int &pci_device_id) {
    device_name = hip_dev_prop_.name;
    gcn_arch_name = hip_dev_prop_.gcnArchName;
    pci_bus_id = hip_dev_prop_.pciBusID;
    pci_domain_id = hip_dev_prop_.pciDomainID;
    pci_device_id = hip_dev_prop_.pciDeviceID;
}


bool RocVideoDecoder::GetOutputSurfaceInfo(OutputSurfaceInfo **surface_info) {
    if (!width_ || !height_) {
        std::cerr << "ERROR: RocVideoDecoderr is not intialized" << std::endl;
        return false;
    }
    *surface_info = &output_surface_info_;
    return true;
}

bool RocVideoDecoder::InitHIP(int device_id) {
    HIP_API_CALL(hipGetDeviceCount(&num_devices_));
    if (num_devices_ < 1) {
        std::cerr << "ERROR: didn't find any GPU!" << std::endl;
        return false;
    }
    if (device_id >= num_devices_) {
        std::cerr << "ERROR: the requested device_id is not found! " << std::endl;
        return false;
    }
    HIP_API_CALL(hipSetDevice(device_id));
    HIP_API_CALL(hipGetDeviceProperties(&hip_dev_prop_, device_id));
    HIP_API_CALL(hipStreamCreate(&hip_stream_));
    return true;
}
