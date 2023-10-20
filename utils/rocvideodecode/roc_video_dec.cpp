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


RocVideoDecoder::RocVideoDecoder(hipCtx_t hip_ctx, bool b_use_device_mem, rocDecVideoCodec codec, int device_id, bool b_low_latency, bool device_frame_pitched, 
              const Rect *p_crop_rect, const Dim *p_resize_dim, bool extract_user_SEI_Message, int max_width, int max_height,
              uint32_t clk_rate,  bool force_zero_latency) : hip_ctx_(hip_ctx), b_use_device_mem_(b_use_device_mem), codec_id_(codec), device_id_{device_id}, 
              b_low_latency_(b_low_latency), b_device_frame_pitched_(device_frame_pitched), b_extract_sei_message_(extract_user_SEI_Message), 
              max_width_ (max_width), max_height_(max_height), b_force_zero_latency_(force_zero_latency) {

    if (!InitHIP(device_id_)) {
        THROW("Failed to initilize the HIP");
    }
    if (p_crop_rect) crop_rect_ = *p_crop_rect;
    if (p_resize_dim) resize_dim_ = *p_resize_dim;
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

static const char * GetVideoCodecString(rocDecVideoCodec eCodec) {
    static struct {
        rocDecVideoCodec eCodec;
        const char *name;
    } aCodecName [] = {
        { rocDecVideoCodec_MPEG1,     "MPEG-1"       },
        { rocDecVideoCodec_MPEG2,     "MPEG-2"       },
        { rocDecVideoCodec_MPEG4,     "MPEG-4 (ASP)" },
        { rocDecVideoCodec_H264,      "AVC/H.264"    },
        { rocDecVideoCodec_JPEG,      "M-JPEG"       },
        { rocDecVideoCodec_HEVC,      "H.265/HEVC"   },
        { rocDecVideoCodec_VP8,       "VP8"          },
        { rocDecVideoCodec_VP9,       "VP9"          },
        { rocDecVideoCodec_AV1,       "AV1"          },
        { rocDecVideoCodec_NumCodecs, "Invalid"      },
    };

    if (eCodec >= 0 && eCodec <= rocDecVideoCodec_NumCodecs) {
        return aCodecName[eCodec].name;
    }
    for (int i = rocDecVideoCodec_NumCodecs + 1; i < sizeof(aCodecName) / sizeof(aCodecName[0]); i++) {
        if (eCodec == aCodecName[i].eCodec) {
            return aCodecName[eCodec].name;
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

static float GetChromaHeightFactor(rocDecVideoSurfaceFormat surface_format)
{
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

static int GetChromaPlaneCount(rocDecVideoSurfaceFormat surface_format)
{
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


/* Return value from HandleVideoSequence() are interpreted as   :
*  0: fail, 1: succeeded, > 1: override dpb size of parser (set by CUVIDPARSERPARAMS::ulMaxNumDecodeSurfaces while creating parser)
*/
int RocVideoDecoder::HandleVideoSequence(RocdecVideoFormat *pVideoFormat) {
    //START_TIMER
    input_video_info_str_.str("");
    input_video_info_str_.clear();
    input_video_info_str_ << "Input Video Information" << std::endl
        << "\tCodec        : " << GetCodecFmtName(pVideoFormat->codec) << std::endl
        << "\tFrame rate   : " << pVideoFormat->frame_rate.numerator << "/" << pVideoFormat->frame_rate.denominator
            << " = " << 1.0 * pVideoFormat->frame_rate.numerator / pVideoFormat->frame_rate.denominator << " fps" << std::endl
        << "\tSequence     : " << (pVideoFormat->progressive_sequence ? "Progressive" : "Interlaced") << std::endl
        << "\tCoded size   : [" << pVideoFormat->coded_width << ", " << pVideoFormat->coded_height << "]" << std::endl
        << "\tDisplay area : [" << pVideoFormat->display_area.left << ", " << pVideoFormat->display_area.top << ", "
            << pVideoFormat->display_area.right << ", " << pVideoFormat->display_area.bottom << "]" << std::endl
        << "\tChroma       : " << GetVideoChromaFormatName(pVideoFormat->chroma_format) << std::endl
        << "\tBit depth    : " << pVideoFormat->bit_depth_luma_minus8 + 8
    ;
    input_video_info_str_ << std::endl;

    int nDecodeSurface = pVideoFormat->min_num_decode_surfaces;

    RocdecDecodeCaps decode_caps;
    memset(&decode_caps, 0, sizeof(decode_caps));
    decode_caps.eCodecType = pVideoFormat->codec;
    decode_caps.eChromaFormat = pVideoFormat->chroma_format;
    decode_caps.nBitDepthMinus8 = pVideoFormat->bit_depth_luma_minus8;

    ROCDEC_API_CALL(rocDecGetDecoderCaps(&decode_caps));

    if(!decode_caps.bIsSupported){
        THROW("Rocdec:: Codec not supported on this GPU: " + TOSTR(ROCDEC_NOT_SUPPORTED));
        return 0;
    }

    if ((pVideoFormat->coded_width > decode_caps.nMaxWidth) ||
        (pVideoFormat->coded_height > decode_caps.nMaxHeight)){

        std::ostringstream errorString;
        errorString << std::endl
                    << "Resolution          : " << pVideoFormat->coded_width << "x" << pVideoFormat->coded_height << std::endl
                    << "Max Supported (wxh) : " << decode_caps.nMaxWidth << "x" << decode_caps.nMaxHeight << std::endl
                    << "Resolution not supported on this GPU ";

        const std::string cErr = errorString.str();
        THROW(cErr+ TOSTR(ROCDEC_NOT_SUPPORTED));
        return nDecodeSurface;
    }

    if (width_ && height_ && chroma_height_) {

        // rocdecCreateDecoder() has been called before, and now there's possible config change
        // todo:: support reconfigure
        //return ReconfigureDecoder(pVideoFormat);
    }

    // eCodec has been set in the constructor (for parser). Here it's set again for potential correction
    codec_id_ = pVideoFormat->codec;
    video_chroma_format_ = pVideoFormat->chroma_format;
    bitdepth_minus_8_ = pVideoFormat->bit_depth_luma_minus8;
    byte_per_pixel_ = bitdepth_minus_8_ > 0 ? 2 : 1;

    // Set the output surface format same as chroma format
    if (video_chroma_format_ == rocDecVideoChromaFormat_420 || rocDecVideoChromaFormat_Monochrome)
        video_surface_format_ = pVideoFormat->bit_depth_luma_minus8 ? rocDecVideoSurfaceFormat_P016 : rocDecVideoSurfaceFormat_NV12;
    else if (video_chroma_format_ == rocDecVideoChromaFormat_444)
        video_surface_format_ = pVideoFormat->bit_depth_luma_minus8 ? rocDecVideoSurfaceFormat_YUV444_16Bit : rocDecVideoSurfaceFormat_YUV444;
    else if (video_chroma_format_ == rocDecVideoChromaFormat_422)
        video_surface_format_ = rocDecVideoSurfaceFormat_NV12;      // 422 output surface is not supported:: default to NV12

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
            THROW("No supported output format found" + TOSTR(ROCDEC_NOT_SUPPORTED));
    }
    video_format_ = *pVideoFormat;

    RocdecDecoderCreateInfo videoDecodeCreateInfo = { 0 };
    videoDecodeCreateInfo.CodecType = pVideoFormat->codec;
    videoDecodeCreateInfo.ChromaFormat = pVideoFormat->chroma_format;
    videoDecodeCreateInfo.OutputFormat = video_surface_format_;
    videoDecodeCreateInfo.bitDepthMinus8 = pVideoFormat->bit_depth_luma_minus8;
    videoDecodeCreateInfo.ulNumOutputSurfaces = 2;
    videoDecodeCreateInfo.ulNumDecodeSurfaces = nDecodeSurface;
    videoDecodeCreateInfo.ulWidth = pVideoFormat->coded_width;
    videoDecodeCreateInfo.ulHeight = pVideoFormat->coded_height;
    // AV1 has max width/height of sequence in sequence header
    if (pVideoFormat->codec == rocDecVideoCodec_AV1 && pVideoFormat->seqhdr_data_length > 0) {
        // dont overwrite if it is already set from cmdline or reconfig.txt
        if (!(max_width_ > pVideoFormat->coded_width || max_height_ > pVideoFormat->coded_height))
        {
            RocdecVideoFormatEx *vidFormatEx = (RocdecVideoFormatEx *)pVideoFormat;
            max_width_ = vidFormatEx->max_width;
            max_height_ = vidFormatEx->max_height;
        }
    }
    if (max_width_ < (int)pVideoFormat->coded_width)
        max_width_ = pVideoFormat->coded_width;
    if (max_height_ < (int)pVideoFormat->coded_height)
        max_height_ = pVideoFormat->coded_height;
    videoDecodeCreateInfo.ulMaxWidth = max_width_;
    videoDecodeCreateInfo.ulMaxHeight = max_height_;

    if (!(crop_rect_.r && crop_rect_.b) && !(resize_dim_.w && resize_dim_.h)) {
        width_ = pVideoFormat->display_area.right - pVideoFormat->display_area.left;
        height_ = pVideoFormat->display_area.bottom - pVideoFormat->display_area.top;
        videoDecodeCreateInfo.ulTargetWidth = pVideoFormat->coded_width;
        videoDecodeCreateInfo.ulTargetHeight = pVideoFormat->coded_height;
    } else {
        if (resize_dim_.w && resize_dim_.h) {
            videoDecodeCreateInfo.display_area.left = pVideoFormat->display_area.left;
            videoDecodeCreateInfo.display_area.top = pVideoFormat->display_area.top;
            videoDecodeCreateInfo.display_area.right = pVideoFormat->display_area.right;
            videoDecodeCreateInfo.display_area.bottom = pVideoFormat->display_area.bottom;
            width_ = resize_dim_.w;
            height_ = resize_dim_.h;
        }

        if (crop_rect_.r && crop_rect_.b) {
            videoDecodeCreateInfo.display_area.left = crop_rect_.l;
            videoDecodeCreateInfo.display_area.top = crop_rect_.t;
            videoDecodeCreateInfo.display_area.right = crop_rect_.r;
            videoDecodeCreateInfo.display_area.bottom = crop_rect_.b;
            width_ = crop_rect_.r - crop_rect_.l;
            height_ = crop_rect_.b - crop_rect_.t;
        }
        videoDecodeCreateInfo.ulTargetWidth = width_;
        videoDecodeCreateInfo.ulTargetHeight = height_;
    }

    chroma_height_ = (int)(ceil(height_ * GetChromaHeightFactor(video_surface_format_)));
    num_chroma_planes_ = GetChromaPlaneCount(video_surface_format_);
    surface_height_ = videoDecodeCreateInfo.ulTargetHeight;
    surface_width_ = videoDecodeCreateInfo.ulTargetWidth;
    surface_stride_ = align(surface_width_, 256) * byte_per_pixel_;      // 256 alignment is enforced for internal VCN surface, keeping the same for ease of memcpy
    // fill output_surface_info_
    output_surface_info_.output_width = surface_width_;
    output_surface_info_.output_height = surface_height_;
    output_surface_info_.output_pitch  = surface_stride_;
    output_surface_info_.bit_depth = bitdepth_minus_8_ + 8;
    output_surface_info_.bytes_per_pixel = byte_per_pixel_;
    output_surface_info_.surface_format = video_surface_format_;
    output_surface_info_.num_chroma_planes = num_chroma_planes_;

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

    ROCDEC_API_CALL(rocDecCreateDecoder(&roc_decoder_, &videoDecodeCreateInfo));
    return nDecodeSurface;
}


int RocVideoDecoder::ReconfigureDecoder(RocdecVideoFormat *pVideoFormat) {
    THROW("ReconfigureDecoder is not supported in this version: " + TOSTR(ROCDEC_NOT_SUPPORTED));
    return ROCDEC_NOT_SUPPORTED;
}

/**
 * @brief 
 * 
 * @param pPicParams 
 * @return int 1: success 0: fail
 */
int RocVideoDecoder::HandlePictureDecode(RocdecPicParams *pPicParams) {
    if (!roc_decoder_)
    {
        THROW("Decoder not initialized: failed with ErrCode: " +  TOSTR(ROCDEC_NOT_INITIALIZED));
        return false;
    }
    pic_num_in_dec_order_[pPicParams->CurrPicIdx] = decode_poc_++;
    ROCDEC_API_CALL(rocDecDecodeFrame(roc_decoder_, pPicParams));
    if (b_force_zero_latency_ && ((!pPicParams->field_pic_flag) || (pPicParams->second_field)))
    {
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
                            case SEI_TYPE_TIME_CODE:
                            {
                                //todo:: check if we need to write timecode
                            }
                            break;
                            case SEI_TYPE_USER_DATA_UNREGISTERED:
                            {
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
            free(sei_message_display_q_[pDispInfo->picture_index].pSEIMessage);
        }
    }

    void * src_dev_ptr[3] = { 0 };
    uint32_t src_pitch[3] = { 0 };
    ROCDEC_API_CALL(rocDecMapVideoFrame(roc_decoder_, pDispInfo->picture_index, src_dev_ptr, &src_pitch, &video_proc_params));

    RocdecDecodeStatus dec_status;
    memset(&dec_status, 0, sizeof(dec_status));
    rocDecStatus result = rocDecGetDecodeStatus(roc_decoder_, pDispInfo->picture_index, &dec_status);
    if (result == ROCDEC_SUCCESS && (dec_status.decodeStatus == rocDecodeStatus_Error || dec_status.decodeStatus == rocDecodeStatus_Error_Concealed)) {
        std::cerr << "Decode Error occurred for picture: " << pic_num_in_dec_order_[pDispInfo->picture_index] << std::endl;
    }
    // copy the decoded surface info device or host
    uint8_t *p_dec_frame = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_vp_frame_);
        if ((unsigned)++decoded_frame_cnt_ > vp_frames_.size()) {
            // Not enough frames in stock
            num_alloced_frames_++;
            DecFrameBuffer dec_frame = { 0 };
            if (b_use_device_mem_) {
                // allocate based on piched or not
                if (b_device_frame_pitched_)
                    HIP_API_CALL(hipMalloc((void **)&dec_frame.frame_ptr, GetFrameSizePitched()));
                else
                    HIP_API_CALL(hipMalloc((void **)&dec_frame.frame_ptr, GetFrameSize()));
            }
            else{
                dec_frame.frame_ptr = new uint8_t[GetFrameSize()];
            }
            dec_frame.pts = pDispInfo->pts;
            vp_frames_.push_back(dec_frame);
        }
        p_dec_frame = vp_frames_[decoded_frame_cnt_ - 1].frame_ptr;
    }

    // Copy luma data
    int dst_pitch = b_device_frame_pitched_? surface_stride_ : width_*byte_per_pixel_;
    if (b_use_device_mem_) {
        if (src_pitch[0] == dst_pitch) {
            int luma_size = src_pitch[0] * height_;
            HIP_API_CALL(hipMemcpyDtoDAsync(p_dec_frame, src_dev_ptr[0], luma_size, hip_stream_));
        }else {
            // use 2d copy to copy an ROI
            HIP_API_CALL(hipMemcpy2DAsync(p_dec_frame, dst_pitch, src_dev_ptr[0], src_pitch[0], width_*byte_per_pixel_, height_, hipMemcpyDeviceToDevice, hip_stream_));
        } 
    }
    else
        HIP_API_CALL(hipMemcpy2DAsync(p_dec_frame, width_*byte_per_pixel_, src_dev_ptr[0], src_pitch[0], width_*byte_per_pixel_, height_, hipMemcpyDeviceToHost, hip_stream_));

    // Copy chroma plane ( )
    // rocDec output gives pointer to luma and chroma pointers seperated for the decoded frame
    uint8_t *p_frame_uv = p_dec_frame + dst_pitch * height_;
    if (b_use_device_mem_) {
        if (src_pitch[1] == dst_pitch) {
            int chroma_size = chroma_height_ * dst_pitch;
            HIP_API_CALL(hipMemcpyDtoDAsync(p_frame_uv, src_dev_ptr[1], chroma_size, hip_stream_));
        }else {
            // use 2d copy to copy an ROI
            HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[1], src_pitch[1], width_*byte_per_pixel_, chroma_height_, hipMemcpyDeviceToDevice, hip_stream_));
        }
    }else
        HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[1], src_pitch[1], width_*byte_per_pixel_, chroma_height_, hipMemcpyDeviceToHost, hip_stream_));

    if (num_chroma_planes_ == 2) {
        uint8_t *p_frame_uv = p_dec_frame + dst_pitch * height_*2;
        if (b_use_device_mem_) {
            if (src_pitch[2] == dst_pitch) {
                int chroma_size = chroma_height_ * dst_pitch;
                HIP_API_CALL(hipMemcpyDtoDAsync(p_frame_uv, src_dev_ptr[2], chroma_size, hip_stream_));
            }else {
                // use 2d copy to copy an ROI
                HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[2], src_pitch[2], width_*byte_per_pixel_, chroma_height_, hipMemcpyDeviceToDevice, hip_stream_));
            }
        }else
            HIP_API_CALL(hipMemcpy2DAsync(p_frame_uv, dst_pitch, src_dev_ptr[2], src_pitch[2], width_*byte_per_pixel_, chroma_height_, hipMemcpyDeviceToHost, hip_stream_));
    }

    HIP_API_CALL(hipStreamSynchronize(hip_stream_));
    ROCDEC_API_CALL(rocDecUnMapVideoFrame(roc_decoder_, src_dev_ptr[0]));
    return 1;
}

int RocVideoDecoder::GetSEIMessage(RocdecSeiMessageInfo *pSEIMessageInfo) {
    uint32_t sei_num_mesages = pSEIMessageInfo->sei_message_count;
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
    return 1;
}


int RocVideoDecoder::DecodeFrame(const uint8_t *data, size_t size, int pkt_flags, int64_t pts) {
    int decoded_frame_cnt_ = 0, decoded_frame_cnt_ret_ = 0;
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
        if (pts) *pts = vp_frames_[decoded_frame_cnt_ret_].pts;
        return vp_frames_[decoded_frame_cnt_ret_++].frame_ptr;
    }
    return nullptr;
}

#if 0 // may be needed for future

void RocVideoDecoder::SaveImage(std::string output_file_name, void *dev_mem, OutputImageInfo *image_info, bool is_output_RGB) {
    uint8_t *hst_ptr = nullptr;
    uint64_t output_image_size = image_info->output_image_size_in_bytes;
    if (hst_ptr == nullptr) {
        hst_ptr = new uint8_t [output_image_size];
    }
    hipError_t hip_status = hipSuccess;
    hip_status = hipMemcpyDtoH((void *)hst_ptr, dev_mem, output_image_size);
    if (hip_status != hipSuccess) {
        std::cout << "ERROR: hipMemcpyDtoH failed! (" << hip_status << ")" << std::endl;
        delete [] hst_ptr;
        return;
    }

    // no RGB dump if the surface type is YUV400
    if (image_info->chroma_format == ROCDEC_FMT_YUV400 && is_output_RGB) {
        return;
    }
    uint8_t *tmp_hst_ptr = hst_ptr;
    if (fp_out_ == nullptr) {
        fp_out_ = fopen(output_file_name.c_str(), "wb");
    }
    if (fp_out_) {
        int img_width = image_info->output_width;
        int img_height = image_info->output_height;
        int output_image_stride =  image_info->output_h_stride;
        if (img_width * image_info->bytes_per_pixel == output_image_stride && img_height == image_info->output_v_stride) {
            fwrite(hst_ptr, 1, output_image_size, fp_out_);
        } else {
            uint32_t width = is_output_RGB ? image_info->output_width * 3 : image_info->output_width;
            if (image_info->bit_depth == 8) {
                for (int i = 0; i < image_info->output_height; i++) {
                    fwrite(tmp_hst_ptr, 1, width, fp_out_);
                    tmp_hst_ptr += output_image_stride;
                }
                if (!is_output_RGB) {
                    // dump chroma
                    uint8_t *uv_hst_ptr = hst_ptr + output_image_stride * image_info->output_v_stride;
                    for (int i = 0; i < img_height >> 1; i++) {
                        fwrite(uv_hst_ptr, 1, width, fp_out_);
                        uv_hst_ptr += output_image_stride;
                    }
                }
            } else if (image_info->bit_depth > 8 &&  image_info->bit_depth <= 16 ) {
                for (int i = 0; i < img_height; i++) {
                    fwrite(tmp_hst_ptr, 1, width * image_info->bytes_per_pixel, fp_out_);
                    tmp_hst_ptr += output_image_stride;
                }
                if (!is_output_RGB) {
                    // dump chroma
                    uint8_t *uv_hst_ptr = hst_ptr + output_image_stride * image_info->output_v_stride;
                    for (int i = 0; i < img_height >> 1; i++) {
                        fwrite(uv_hst_ptr, 1, width * image_info->bytes_per_pixel, fp_out_);
                        uv_hst_ptr += output_image_stride;
                    }
                }
            }
        }
    }

    if (hst_ptr != nullptr) {
        delete [] hst_ptr;
        hst_ptr = nullptr;
        tmp_hst_ptr = nullptr;
    }
}
#endif

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

bool RocVideoDecoder::InitHIP(int device_id)
{
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
