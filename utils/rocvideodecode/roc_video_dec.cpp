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

RocVideoDecode::RocVideoDecode(int device_id) : num_devices_{0}, device_id_{device_id}, hip_stream_ {0},
    external_mem_handle_desc_{{}}, external_mem_buffer_desc_{{}}, va_display_{nullptr},
    va_profiles_{{}}, num_va_profiles_{-1}, yuv_dev_mem_{nullptr}, width_{0}, height_{0},
    chroma_height_{0}, surface_height_{0}, surface_width_{0}, num_chroma_planes_{0},
    surface_stride_{0}, surface_size_{0}, bit_depth_{8}, byte_per_pixel_{1},
    subsampling_{ROCDEC_FMT_YUV420}, out_image_info_{{}}, fp_out_{nullptr}, drm_fd_{-1} {

    if (!InitHIP(device_id_)) {
        THROW("Failed to initilize the HIP");
    }
}

RocVideoDecode::RocVideoDecode(hipCtx_t hip_ctx, bool b_use_device_mem, rocDecVideoCodec codec, int device_id, bool b_low_latency, bool device_frame_pitched, 
              const Rect *p_crop_rect, const Dim *p_resize_dim, bool extract_user_SEI_Message, int max_width, int max_height,
              uint32_t clk_rate,  bool force_zero_latency) : hip_ctx_(hip_ctx), b_use_device_mem_(b_use_device_mem), codec_id_(codec), device_id_{device_id}, 
              b_low_latency_(b_low_latency), b_extract_sei_message_(extract_user_SEI_Message), max_width_ (max_width), max_height_(max_height),
              b_force_zero_latency_(force_zero_latency)
{
    if (!InitHIP(device_id_)) {
        THROW("Failed to initilize the HIP");
    }
    if (p_crop_rect) crop_rect_ = *p_crop_rect;
    if (p_resize_dim) resize_dim_ = *p_resize_dim;
    if (b_extract_sei_message_)
    {
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


RocVideoDecode::~RocVideoDecode() {
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
const char *RocVideoDecode::GetCodecFmtName(rocDecVideoCodec codec_id)
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

    if (e_chroma_format >= 0 && e_chroma_format < sizeof(chroma_fmt) / sizeof(ChromaFormatName[0])) {
        return ChromaFormatName[e_chroma_format].name;
    }
    return "Unknown";
}



std::string RocVideoDecode::GetPixFmtName(RocDecImageFormat subsampling) {
    std::string fmt_name = "";
    switch (subsampling) {
        case ROCDEC_FMT_YUV420:
            fmt_name = "YUV420";
            break;
        case ROCDEC_FMT_YUV444:
            fmt_name = "YUV444";
            break;
        case ROCDEC_FMT_YUV422:
            fmt_name = "YUV422";
            break;
        case ROCDEC_FMT_YUV400:
            fmt_name = "YUV400";
            break;
       case ROCDEC_FMT_YUV420P10:
            fmt_name = "YUV420P10";
            break;
       case ROCDEC_FMT_YUV420P12:
            fmt_name = "YUV420P12";
            break;
        case ROCDEC_FMT_RGB:
            fmt_name = "RGB";
            break;
        default:
            std::cerr << "ERROR: subsampling format is not supported!" << std::endl;
    }
    return fmt_name;
}

/* Return value from HandleVideoSequence() are interpreted as   :
*  0: fail, 1: succeeded, > 1: override dpb size of parser (set by CUVIDPARSERPARAMS::ulMaxNumDecodeSurfaces while creating parser)
*/
int RocVideoDecode::HandleVideoSequence(RocdecVideoFormat *pVideoFormat) {
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
    BPP_ = bitdepth_minus_8_ > 0 ? 2 : 1;

    // Set the output surface format same as chroma format
    if (video_chroma_format_ == rocDecVideoChromaFormat_420 || rocDecVideoChromaFormat_Monochrome)
        video_surface_format_ = pVideoFormat->bit_depth_luma_minus8 ? rocDecVideoSurfaceFormat_P016 : rocDecVideoSurfaceFormat_NV12;
    else if (video_chroma_format_ == rocDecVideoChromaFormat_444)
        video_surface_format_ = pVideoFormat->bit_depth_luma_minus8 ? rocDecVideoSurfaceFormat_YUV444_16Bit : rocDecVideoSurfaceFormat_YUV444;
    else if (video_chroma_format_ == rocDecVideoChromaFormat_422)
        video_surface_format_ = rocDecVideoSurfaceFormat_NV12;      // 422 output surface is not supported:: default to NV12

    // Check if output format supported. If not, check falback options
    if (!(decode_caps.nOutputFormatMask & (1 << m_eOutputFormat)))
    {
        if (decode_caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_NV12))
            m_eOutputFormat = cudaVideoSurfaceFormat_NV12;
        else if (decode_caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_P016))
            m_eOutputFormat = cudaVideoSurfaceFormat_P016;
        else if (decode_caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444))
            m_eOutputFormat = cudaVideoSurfaceFormat_YUV444;
        else if (decode_caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444_16Bit))
            m_eOutputFormat = cudaVideoSurfaceFormat_YUV444_16Bit;
        else 
            NVDEC_THROW_ERROR("No supported output format found", CUDA_ERROR_NOT_SUPPORTED);
    }
    m_videoFormat = *pVideoFormat;

    CUVIDDECODECREATEINFO videoDecodeCreateInfo = { 0 };
    videoDecodeCreateInfo.CodecType = pVideoFormat->codec;
    videoDecodeCreateInfo.ChromaFormat = pVideoFormat->chroma_format;
    videoDecodeCreateInfo.OutputFormat = m_eOutputFormat;
    videoDecodeCreateInfo.bitDepthMinus8 = pVideoFormat->bit_depth_luma_minus8;
    if (pVideoFormat->progressive_sequence)
        videoDecodeCreateInfo.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    else
        videoDecodeCreateInfo.DeinterlaceMode = cudaVideoDeinterlaceMode_Adaptive;
    videoDecodeCreateInfo.ulNumOutputSurfaces = 2;
    // With PreferCUVID, JPEG is still decoded by CUDA while video is decoded by NVDEC hardware
    videoDecodeCreateInfo.ulCreationFlags = cudaVideoCreate_PreferCUVID;
    videoDecodeCreateInfo.ulNumDecodeSurfaces = nDecodeSurface;
    videoDecodeCreateInfo.vidLock = m_ctxLock;
    videoDecodeCreateInfo.ulWidth = pVideoFormat->coded_width;
    videoDecodeCreateInfo.ulHeight = pVideoFormat->coded_height;
    // AV1 has max width/height of sequence in sequence header
    if (pVideoFormat->codec == cudaVideoCodec_AV1 && pVideoFormat->seqhdr_data_length > 0)
    {
        // dont overwrite if it is already set from cmdline or reconfig.txt
        if (!(m_nMaxWidth > pVideoFormat->coded_width || m_nMaxHeight > pVideoFormat->coded_height))
        {
            CUVIDEOFORMATEX *vidFormatEx = (CUVIDEOFORMATEX *)pVideoFormat;
            m_nMaxWidth = vidFormatEx->av1.max_width;
            m_nMaxHeight = vidFormatEx->av1.max_height;
        }
    }
    if (m_nMaxWidth < (int)pVideoFormat->coded_width)
        m_nMaxWidth = pVideoFormat->coded_width;
    if (m_nMaxHeight < (int)pVideoFormat->coded_height)
        m_nMaxHeight = pVideoFormat->coded_height;
    videoDecodeCreateInfo.ulMaxWidth = m_nMaxWidth;
    videoDecodeCreateInfo.ulMaxHeight = m_nMaxHeight;

    if (!(m_cropRect.r && m_cropRect.b) && !(m_resizeDim.w && m_resizeDim.h)) {
        m_nWidth = pVideoFormat->display_area.right - pVideoFormat->display_area.left;
        m_nLumaHeight = pVideoFormat->display_area.bottom - pVideoFormat->display_area.top;
        videoDecodeCreateInfo.ulTargetWidth = pVideoFormat->coded_width;
        videoDecodeCreateInfo.ulTargetHeight = pVideoFormat->coded_height;
    } else {
        if (m_resizeDim.w && m_resizeDim.h) {
            videoDecodeCreateInfo.display_area.left = pVideoFormat->display_area.left;
            videoDecodeCreateInfo.display_area.top = pVideoFormat->display_area.top;
            videoDecodeCreateInfo.display_area.right = pVideoFormat->display_area.right;
            videoDecodeCreateInfo.display_area.bottom = pVideoFormat->display_area.bottom;
            m_nWidth = m_resizeDim.w;
            m_nLumaHeight = m_resizeDim.h;
        }

        if (m_cropRect.r && m_cropRect.b) {
            videoDecodeCreateInfo.display_area.left = m_cropRect.l;
            videoDecodeCreateInfo.display_area.top = m_cropRect.t;
            videoDecodeCreateInfo.display_area.right = m_cropRect.r;
            videoDecodeCreateInfo.display_area.bottom = m_cropRect.b;
            m_nWidth = m_cropRect.r - m_cropRect.l;
            m_nLumaHeight = m_cropRect.b - m_cropRect.t;
        }
        videoDecodeCreateInfo.ulTargetWidth = m_nWidth;
        videoDecodeCreateInfo.ulTargetHeight = m_nLumaHeight;
    }

    m_nChromaHeight = (int)(ceil(m_nLumaHeight * GetChromaHeightFactor(m_eOutputFormat)));
    m_nNumChromaPlanes = GetChromaPlaneCount(m_eOutputFormat);
    m_nSurfaceHeight = videoDecodeCreateInfo.ulTargetHeight;
    m_nSurfaceWidth = videoDecodeCreateInfo.ulTargetWidth;
    m_displayRect.b = videoDecodeCreateInfo.display_area.bottom;
    m_displayRect.t = videoDecodeCreateInfo.display_area.top;
    m_displayRect.l = videoDecodeCreateInfo.display_area.left;
    m_displayRect.r = videoDecodeCreateInfo.display_area.right;

    input_video_info_str_ << "Video Decoding Params:" << std::endl
        << "\tNum Surfaces : " << videoDecodeCreateInfo.ulNumDecodeSurfaces << std::endl
        << "\tCrop         : [" << videoDecodeCreateInfo.display_area.left << ", " << videoDecodeCreateInfo.display_area.top << ", "
        << videoDecodeCreateInfo.display_area.right << ", " << videoDecodeCreateInfo.display_area.bottom << "]" << std::endl
        << "\tResize       : " << videoDecodeCreateInfo.ulTargetWidth << "x" << videoDecodeCreateInfo.ulTargetHeight << std::endl
        << "\tDeinterlace  : " << std::vector<const char *>{"Weave", "Bob", "Adaptive"}[videoDecodeCreateInfo.DeinterlaceMode]
    ;
    input_video_info_str_ << std::endl;

    NVDEC_API_CALL(cuvidCreateDecoder(&m_hDecoder, &videoDecodeCreateInfo));
    NvDecoder::addDecoderSessionOverHead(getDecoderSessionID(), elapsedTime);
    return nDecodeSurface;
}


int RocVideoDecode::ReconfigureDecoder(RocdecVideoFormat *pVideoFormat)
{
    THROW("ReconfigureDecoder is not supported in this version: " + TOSTR(errorCode));
    return ROCDEC_NOT_SUPPORTED;
}

/**
 * @brief 
 * 
 * @param pPicParams 
 * @return int 1: success 0: fail
 */
int RocVideoDecode::HandlePictureDecode(RocdecPicParams *pPicParams) {
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
int RocVideoDecode::HandlePictureDisplay(RocdecParserDispInfo *pDispInfo) {
    RocdecProcParams video_proc_params = {};
    video_proc_params.progressive_frame = pDispInfo->progressive_frame;
    video_proc_params.second_field = pDispInfo->repeat_first_field + 1;
    video_proc_params.top_field_first = pDispInfo->top_field_first;
    video_proc_params.unpaired_field = pDispInfo->repeat_first_field < 0;
    video_proc_params.output_stream = hip_stream_;

    if (b_extract_sei_message_)
    {
        if (sei_message_display_q_[pDispInfo->picture_index].pSEIData)
        {
            // Write SEI Message
            uint8_t *sei_buffer = (uint8_t *)(sei_message_display_q_[pDispInfo->picture_index].pSEIData);
            uint32_t sei_num_messages = sei_message_display_q_[pDispInfo->picture_index].sei_message_count;
            RocdecSeiMessage *sei_message = sei_message_display_q_[pDispInfo->picture_index].pSEIMessage;
            if (fp_sei_)
            {
                for (uint32_t i = 0; i < sei_num_messages; i++)
                {
                    if (codec_id_ == rocDecVideoCodec_H264 || rocDecVideoCodec_HEVC)
                    {    
                        switch (sei_message[i].sei_message_type)
                        {
                            case SEI_TYPE_TIME_CODE:
                            {
                                //todo:: check if we need to write timecode
                            }
                            break;
                            case SEI_TYPE_USER_DATA_UNREGISTERED:
                            {
                                fwrite(seiBuffer, sei_message[i].sei_message_size, 1, fp_sei_);
                            }
                            break;
                        }            
                    }
                    if (m_eCodec == rocDecVideoCodec_AV1)
                    {
                        fwrite(seiBuffer, sei_message[i].sei_message_size, 1, m_fpSEI);
                    }    
                    seiBuffer += sei_message[i].sei_message_size;
                }
            }
            free(sei_message_display_q_[pDispInfo->picture_index].pSEIData);
            free(sei_message_display_q_[pDispInfo->picture_index].pSEIMessage);
        }
    }

    void * src_dev_ptr[3] = { 0 };
    unsigned int src_pitch[3] = { 0 };
    ROCDEC_API_CALL(rocDecMapVideoFrame(roc_decoder_, pDispInfo->picture_index, &src_dev_ptr, &src_pitch, &video_proc_params));

    RocdecDecodeStatus dec_status;
    memset(&dec_status, 0, sizeof(dec_status));
    rocDecStatus result = rocDecGetDecodeStatus(roc_decoder_, pDispInfo->picture_index, &dec_status);
    if (result == ROCDEC_SUCCESS && (dec_status.decodeStatus == rocDecodeStatus_Error || dec_status.decodeStatus == rocDecodeStatus_Error_Concealed))
    {
        std::cerr("Decode Error occurred for picture %d\n", pic_num_in_dec_order_[pDispInfo->picture_index]);
    }
    // copy the decoded surface info device or host
    uint8_t *p_dec_frame = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_vp_frame_);
        if ((unsigned)++decoded_frame_cnt_ > vp_frames_.size())
        {
            // Not enough frames in stock
            num_alloced_frames_++;
            DecFrameBuffer dec_frame = { 0 };
            if (b_use_device_mem_)
            {
                // device memory pointer is always pitched
                HIP_API_CALL(hipMalloc((void **)&dec_frame.frame_ptr, GetFrameSizePitched()));
            }
            else
            {
                dec_frame.frame_ptr = new uint8_t[GetFrameSize()];
            }
            dec_frame.pts = pDispInfo->timestamp;
            vp_frames_.push_back(dec_frame);
        }
        p_dec_frame = vp_frames_[decoded_frame_cnt_ - 1];
    }

    // Copy luma data
    int luma_size = surface_width_ * height_ * byte_per_pixel_;
    HIP_API_CALL(hipMemcpyAsync(p_dec_frame, src_dev_ptr[0], luma_size, hip_stream_));

    // Copy chroma plane ( )
    // rocDec output gives pointer to luma and chroma pointers seperated for the decoded frame
    uint8_t *p_frame_uv = p_dec_frame + surface_width_ * height_ * byte_per_pixel_;
    int chroma_size = chroma_height_ * surface_width_ * byte_per_pixel_;
    HIP_API_CALL(hipMemcpyAsync(p_frame_uv, src_dev_ptr[1], chroma_size, hip_stream_));
    HIP_API_CALL(hipStreamSynchronize(hip_stream_));
    ROCDEC_API_CALL(rocDecUnmapVideoFrame(roc_decoder_, src_dev_ptr[0]));
    return 1;
}


int RocVideoDecode::DecodeFrame(const uint8_t *data, size_t size, int pkt_flags, int64_t pts) {
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

uint8_t* RocVideoDecode::GetFrame(int64_t *pts) {
    if (decoded_frame_cnt_ > 0) {
        std::lock_guard<std::mutex> lock(mtx_vp_frame_);
        decoded_frame_cnt_--;
        if (pTimestamp)
            *pTimestamp = vp_frames_[decoded_frame_cnt_ret_].pts;
        return m_vpFrame[decoded_frame_cnt_ret_++].frame_ptr;
    }
    return nullptr;
}

void RocVideoDecode::SaveImage(std::string output_file_name, void *dev_mem, OutputImageInfo *image_info, bool is_output_RGB) {
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

void RocVideoDecode::GetDeviceinfo(std::string &device_name, std::string &gcn_arch_name, int &pci_bus_id, int &pci_domain_id, int &pci_device_id, std::string &drm_node) {
    device_name = hip_dev_prop_.name;
    gcn_arch_name = hip_dev_prop_.gcnArchName;
    pci_bus_id = hip_dev_prop_.pciBusID;
    pci_domain_id = hip_dev_prop_.pciDomainID;
    pci_device_id = hip_dev_prop_.pciDeviceID;
    drm_node = drm_nodes_[device_id_];
}


bool RocVideoDecode::GetOutputImageInfo(OutputImageInfo **image_info) {
    if (!width_ || !height_) {
        std::cerr << "ERROR: RocVideoDecoder is not intialized" << std::endl;
        return false;
    }
    *image_info = &out_image_info_;
    return true;
}

bool RocVideoDecode::InitHIP(int device_id)
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

void RocVideoDecode::InitDRMnodes() {
    // build the DRM render node names
    for (int i = 0; i < num_devices_; i++) {
        drm_nodes_.push_back("/dev/dri/renderD" + std::to_string(128 + i));
    }
}


bool RocVideoDecode::GetImageSizeHintInternal(RocDecImageFormat subsampling, const uint32_t width, const uint32_t height, uint32_t *output_stride, size_t *output_image_size) {
    if ( output_stride == nullptr || output_image_size == nullptr) {
        std::cerr << "invalid input parameters!" << std::endl;
        return false;
    }
    int aligned_height = 0;
    switch (subsampling) {
        case ROCDEC_FMT_YUV420:
            *output_stride = align(width, 256);
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * (aligned_height + (aligned_height >> 1));
             break;
        case ROCDEC_FMT_YUV444:
            *output_stride = align(width, 256);
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * aligned_height * 3;
            break;
        case ROCDEC_FMT_YUV400:
            *output_stride = align(width, 256);
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * aligned_height;
            break;
        case ROCDEC_FMT_RGB:
            *output_stride = align(width, 256) * 3;
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * aligned_height;
            break;
        case ROCDEC_FMT_YUV420P10:
            *output_stride = align(width, 128) * 2;
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * (aligned_height + (aligned_height >> 1));
            break;
        case ROCDEC_FMT_YUV420P12:
            *output_stride = align(width, 128) * 2;
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * (aligned_height + (aligned_height >> 1));
            break;
        default:
            std::cerr << "ERROR: "<< GetPixFmtName(subsampling) <<" is not supported! " << std::endl;
            return false;
    }

    return true;

}