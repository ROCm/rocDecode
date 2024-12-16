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

#include "vaapi_videodecoder.h"

VaapiVideoDecoder::VaapiVideoDecoder(RocDecoderCreateInfo &decoder_create_info) : decoder_create_info_{decoder_create_info},
    drm_fd_{-1}, va_display_{0}, va_config_attrib_{{}}, va_config_id_{0}, va_profile_ {VAProfileNone}, va_context_id_{0}, va_surface_ids_{{}},
    pic_params_buf_id_{0}, iq_matrix_buf_id_{0}, num_slices_{0}, slice_data_buf_id_{0} {
};

VaapiVideoDecoder::~VaapiVideoDecoder() {
    if (drm_fd_ != -1) {
        close(drm_fd_);
    }
    if (va_display_) {
        rocDecStatus rocdec_status = ROCDEC_SUCCESS;
        rocdec_status = DestroyDataBuffers();
        if (rocdec_status != ROCDEC_SUCCESS) {
            ERR("DestroyDataBuffers failed");
        }
        VAStatus va_status = VA_STATUS_SUCCESS;
        va_status = vaDestroySurfaces(va_display_, va_surface_ids_.data(), va_surface_ids_.size());
        if (va_status != VA_STATUS_SUCCESS) {
            ERR("vaDestroySurfaces failed");
        }
        if (va_context_id_)
            va_status = vaDestroyContext(va_display_, va_context_id_);
            if (va_status != VA_STATUS_SUCCESS) {
                ERR("vaDestroyContext failed");
            }
        if (va_config_id_)
            va_status = vaDestroyConfig(va_display_, va_config_id_);
            if (va_status != VA_STATUS_SUCCESS) {
                ERR("vaDestroyConfig failed");
            }
        va_status = vaTerminate(va_display_);
        if (va_status != VA_STATUS_SUCCESS) {
            ERR("vaTerminate failed");
        }
    }
}

bool VaapiVideoDecoder::IsCodecConfigSupported(int device_id, rocDecVideoCodec codec_type, rocDecVideoChromaFormat chroma_format, uint32_t bit_depth_minus8, rocDecVideoSurfaceFormat output_format) {
    RocdecDecodeCaps decode_caps;
    decode_caps.device_id = device_id;
    decode_caps.codec_type = codec_type;
    decode_caps.chroma_format = chroma_format;
    decode_caps.bit_depth_minus_8 = bit_depth_minus8;
    if((rocDecGetDecoderCaps(&decode_caps) != ROCDEC_SUCCESS) || (decode_caps.is_supported == false) || ((decode_caps.output_format_mask & (1 << output_format)) == 0)) {
        return false;
    } else {
        return true;
    }
}

rocDecStatus VaapiVideoDecoder::InitializeDecoder(std::string device_name, std::string gcn_arch_name) {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;

    // Before initializing the VAAPI, first check to see if the requested codec config is supported
    if (!IsCodecConfigSupported(decoder_create_info_.device_id, decoder_create_info_.codec_type, decoder_create_info_.chroma_format,
        decoder_create_info_.bit_depth_minus_8, decoder_create_info_.output_format)) {
        ERR("The codec config combination is not supported.");
        return ROCDEC_NOT_SUPPORTED;
    }

    // Jefftest
    GpuVaContext& va_ctx = GpuVaContext::GetInstance();
    va_ctx.Initialize(decoder_create_info_.device_id);
    va_display_ = va_ctx.va_display_;
    rocdec_status = CreateDecoderConfig();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("Failed to create a VAAPI decoder configuration.");
        return rocdec_status;
    }
    rocdec_status = CreateSurfaces();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("Failed to create VAAPI surfaces.");
        return rocdec_status;
    }
    rocdec_status = CreateContext();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("Failed to create a VAAPI context.");
        return rocdec_status;
    }
    return rocdec_status;
}

rocDecStatus VaapiVideoDecoder::CreateDecoderConfig() {
    switch (decoder_create_info_.codec_type) {
        case rocDecVideoCodec_HEVC:
            if (decoder_create_info_.bit_depth_minus_8 == 0) {
                va_profile_ = VAProfileHEVCMain;
            } else if (decoder_create_info_.bit_depth_minus_8 == 2) {
                va_profile_ = VAProfileHEVCMain10;
            }
            break;
        case rocDecVideoCodec_AVC:
            va_profile_ = VAProfileH264Main;
            break;
        case rocDecVideoCodec_VP9:
            if (decoder_create_info_.bit_depth_minus_8 == 0) {
                va_profile_ = VAProfileVP9Profile0;
            } else if (decoder_create_info_.bit_depth_minus_8 == 2) {
                va_profile_ = VAProfileVP9Profile2;
            }
            break;
        case rocDecVideoCodec_AV1:
#if VA_CHECK_VERSION(1,6,0)
            va_profile_ = VAProfileAV1Profile0;
#else
            va_profile_ = static_cast<VAProfile>(32); // VAProfileAV1Profile0;
#endif
            break;
        default:
            ERR("The codec type is not supported.");
            return ROCDEC_NOT_SUPPORTED;
    }
    va_config_attrib_.type = VAConfigAttribRTFormat;
    CHECK_VAAPI(vaGetConfigAttributes(va_display_, va_profile_, VAEntrypointVLD, &va_config_attrib_, 1));
    CHECK_VAAPI(vaCreateConfig(va_display_, va_profile_, VAEntrypointVLD, &va_config_attrib_, 1, &va_config_id_));

    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::CreateSurfaces() {
    if (decoder_create_info_.num_decode_surfaces < 1) {
        ERR("Invalid number of decode surfaces.");
        return ROCDEC_INVALID_PARAMETER;
    }
    va_surface_ids_.resize(decoder_create_info_.num_decode_surfaces);
    VASurfaceAttrib surf_attrib;
    surf_attrib.type = VASurfaceAttribPixelFormat;
    surf_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surf_attrib.value.type = VAGenericValueTypeInteger;
    uint32_t surface_format;
    switch (decoder_create_info_.chroma_format) {
        case rocDecVideoChromaFormat_Monochrome:
            surface_format = VA_RT_FORMAT_YUV400;
            surf_attrib.value.value.i = VA_FOURCC_Y800;
            break;
        case rocDecVideoChromaFormat_420:
            if (decoder_create_info_.bit_depth_minus_8 == 2) {
                surface_format = VA_RT_FORMAT_YUV420_10;
                surf_attrib.value.value.i = VA_FOURCC_P010;
            } else if (decoder_create_info_.bit_depth_minus_8 == 4) {
                surface_format = VA_RT_FORMAT_YUV420_12;
#if VA_CHECK_VERSION(1,8,0)
                surf_attrib.value.value.i = VA_FOURCC_P012;
#else
                surf_attrib.value.value.i = 0x32313050; // VA_FOURCC_P012
#endif
            } else {
                surface_format = VA_RT_FORMAT_YUV420;
                surf_attrib.value.value.i = VA_FOURCC_NV12;
            }
            break;
        case rocDecVideoChromaFormat_422:
            surface_format = VA_RT_FORMAT_YUV422;
            break;
        case rocDecVideoChromaFormat_444:
            surface_format = VA_RT_FORMAT_YUV444;
            break;
        default:
            ERR("The surface type is not supported");
            return ROCDEC_NOT_SUPPORTED;
    }
    CHECK_VAAPI(vaCreateSurfaces(va_display_, surface_format, decoder_create_info_.width,
        decoder_create_info_.height, va_surface_ids_.data(), va_surface_ids_.size(), &surf_attrib, 1));
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::CreateContext() {
    CHECK_VAAPI(vaCreateContext(va_display_, va_config_id_, decoder_create_info_.width, decoder_create_info_.height,
        VA_PROGRESSIVE, va_surface_ids_.data(), va_surface_ids_.size(), &va_context_id_));
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::DestroyDataBuffers() {
    if (pic_params_buf_id_) {
        CHECK_VAAPI(vaDestroyBuffer(va_display_, pic_params_buf_id_));
        pic_params_buf_id_ = 0;
    }
    if (iq_matrix_buf_id_) {
        CHECK_VAAPI(vaDestroyBuffer(va_display_, iq_matrix_buf_id_));
        iq_matrix_buf_id_ = 0;
    }
    for (int i = 0; i < num_slices_; i++) {
        if (slice_params_buf_id_[i]) {
            CHECK_VAAPI(vaDestroyBuffer(va_display_, slice_params_buf_id_[i]));
            slice_params_buf_id_[i] = 0;
        }
    }
    if (slice_data_buf_id_) {
        CHECK_VAAPI(vaDestroyBuffer(va_display_, slice_data_buf_id_));
        slice_data_buf_id_ = 0;
    }
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::SubmitDecode(RocdecPicParams *pPicParams) {
    void *pic_params_ptr, *iq_matrix_ptr, *slice_params_ptr;
    uint32_t pic_params_size, iq_matrix_size, slice_params_size;
    bool scaling_list_enabled = false;
    VASurfaceID curr_surface_id;

    // Get the surface id for the current picture, assuming 1:1 mapping between DPB and VAAPI decoded surfaces.
    if (pPicParams->curr_pic_idx >= va_surface_ids_.size() || pPicParams->curr_pic_idx < 0) {
        ERR("curr_pic_idx exceeded the VAAPI surface pool limit.");
        return ROCDEC_INVALID_PARAMETER;
    }
    curr_surface_id = va_surface_ids_[pPicParams->curr_pic_idx];

    // Upload data buffers
    switch (decoder_create_info_.codec_type) {
        case rocDecVideoCodec_HEVC: {
            pPicParams->pic_params.hevc.curr_pic.pic_idx = curr_surface_id;
            for (int i = 0; i < 15; i++) {
                if (pPicParams->pic_params.hevc.ref_frames[i].pic_idx != 0xFF) {
                    if (pPicParams->pic_params.hevc.ref_frames[i].pic_idx >= va_surface_ids_.size() || pPicParams->pic_params.hevc.ref_frames[i].pic_idx < 0) {
                        ERR("Reference frame index exceeded the VAAPI surface pool limit.");
                        return ROCDEC_INVALID_PARAMETER;
                    }
                    pPicParams->pic_params.hevc.ref_frames[i].pic_idx = va_surface_ids_[pPicParams->pic_params.hevc.ref_frames[i].pic_idx];
                }
            }
            pic_params_ptr = (void*)&pPicParams->pic_params.hevc;
            pic_params_size = sizeof(RocdecHevcPicParams);

            if (pPicParams->pic_params.hevc.pic_fields.bits.scaling_list_enabled_flag) {
                scaling_list_enabled = true;
                iq_matrix_ptr = (void*)&pPicParams->iq_matrix.hevc;
                iq_matrix_size = sizeof(RocdecHevcIQMatrix);
            }

            slice_params_ptr = (void*)pPicParams->slice_params.hevc;
            slice_params_size = sizeof(RocdecHevcSliceParams);

            if ((pic_params_size != sizeof(VAPictureParameterBufferHEVC)) || (scaling_list_enabled && (iq_matrix_size != sizeof(VAIQMatrixBufferHEVC))) || 
                (slice_params_size != sizeof(VASliceParameterBufferHEVC))) {
                    ERR("HEVC data_buffer parameter_size not matching vaapi parameter buffer size.");
                    return ROCDEC_RUNTIME_ERROR;
            }
            break;
        }

        case rocDecVideoCodec_AVC: {
            pPicParams->pic_params.avc.curr_pic.pic_idx = curr_surface_id;
            for (int i = 0; i < 16; i++) {
                if (pPicParams->pic_params.avc.ref_frames[i].pic_idx != 0xFF) {
                    if (pPicParams->pic_params.avc.ref_frames[i].pic_idx >= va_surface_ids_.size() || pPicParams->pic_params.avc.ref_frames[i].pic_idx < 0) {
                        ERR("Reference frame index exceeded the VAAPI surface pool limit.");
                        return ROCDEC_INVALID_PARAMETER;
                    }
                    pPicParams->pic_params.avc.ref_frames[i].pic_idx = va_surface_ids_[pPicParams->pic_params.avc.ref_frames[i].pic_idx];
                }
            }
            pic_params_ptr = (void*)&pPicParams->pic_params.avc;
            pic_params_size = sizeof(RocdecAvcPicParams);

            scaling_list_enabled = true;
            iq_matrix_ptr = (void*)&pPicParams->iq_matrix.avc;
            iq_matrix_size = sizeof(RocdecAvcIQMatrix);

            slice_params_ptr = (void*)pPicParams->slice_params.avc;
            slice_params_size = sizeof(RocdecAvcSliceParams);

            if ((pic_params_size != sizeof(VAPictureParameterBufferH264)) || (iq_matrix_size != sizeof(VAIQMatrixBufferH264)) || (slice_params_size != sizeof(VASliceParameterBufferH264))) {
                    ERR("AVC data_buffer parameter_size not matching vaapi parameter buffer size.");
                    return ROCDEC_RUNTIME_ERROR;
            }
            break;
        }

        case rocDecVideoCodec_VP9: {
            for (int i = 0; i < 8; i++) {
                if (pPicParams->pic_params.vp9.reference_frames[i] != 0xFF) {
                    if (pPicParams->pic_params.vp9.reference_frames[i] >= va_surface_ids_.size()) {
                        ERR("Reference frame index exceeded the VAAPI surface pool limit.");
                        return ROCDEC_INVALID_PARAMETER;
                    }
                    pPicParams->pic_params.vp9.reference_frames[i] = va_surface_ids_[pPicParams->pic_params.vp9.reference_frames[i]];
                }
            }
            pic_params_ptr = (void*)&pPicParams->pic_params.vp9;
            pic_params_size = sizeof(RocdecVp9PicParams);
            slice_params_ptr = (void*)pPicParams->slice_params.vp9;
            slice_params_size = sizeof(RocdecVp9SliceParams);
            if ((pic_params_size != sizeof(VADecPictureParameterBufferVP9)) || (slice_params_size != sizeof(VASliceParameterBufferVP9))) {
                    ERR("VP9 data_buffer parameter_size not matching vaapi parameter buffer size.");
                    return ROCDEC_RUNTIME_ERROR;
            }
            break;
        }

        case rocDecVideoCodec_AV1: {
            pPicParams->pic_params.av1.current_frame = curr_surface_id;

            if (pPicParams->pic_params.av1.current_display_picture != 0xFF) {
                if (pPicParams->pic_params.av1.current_display_picture >= va_surface_ids_.size() || pPicParams->pic_params.av1.current_display_picture < 0) {
                    ERR("Current display picture index exceeded the VAAPI surface pool limit.");
                    return ROCDEC_INVALID_PARAMETER;
                }
                pPicParams->pic_params.av1.current_display_picture = va_surface_ids_[pPicParams->pic_params.av1.current_display_picture];
            }

            for (int i = 0; i < pPicParams->pic_params.av1.anchor_frames_num; i++) {
                if (pPicParams->pic_params.av1.anchor_frames_list[i] >= va_surface_ids_.size() || pPicParams->pic_params.av1.anchor_frames_list[i] < 0) {
                    ERR("Anchor frame index exceeded the VAAPI surface pool limit.");
                    return ROCDEC_INVALID_PARAMETER;
                }
                pPicParams->pic_params.av1.anchor_frames_list[i] = va_surface_ids_[pPicParams->pic_params.av1.anchor_frames_list[i]];
            }

            for (int i = 0; i < 8; i++) {
                if (pPicParams->pic_params.av1.ref_frame_map[i] != 0xFF) {
                    if (pPicParams->pic_params.av1.ref_frame_map[i] >= va_surface_ids_.size() || pPicParams->pic_params.av1.ref_frame_map[i] < 0) {
                        ERR("Reference frame index exceeded the VAAPI surface pool limit.");
                        return ROCDEC_INVALID_PARAMETER;
                    }
                    pPicParams->pic_params.av1.ref_frame_map[i] = va_surface_ids_[pPicParams->pic_params.av1.ref_frame_map[i]];
                }
            }

            pic_params_ptr = (void*)&pPicParams->pic_params.av1;
            pic_params_size = sizeof(RocdecAv1PicParams);

            slice_params_ptr = (void*)pPicParams->slice_params.av1;
            slice_params_size = sizeof(RocdecAv1SliceParams);

#if VA_CHECK_VERSION(1,6,0)
            if ((pic_params_size != sizeof(VADecPictureParameterBufferAV1)) || (slice_params_size != sizeof(VASliceParameterBufferAV1))) {
                    ERR("AV1 data_buffer parameter_size not matching vaapi parameter buffer size.");
                    return ROCDEC_RUNTIME_ERROR;
            }
#else
            if ((pic_params_size != 1160) || (slice_params_size != 40)) {
                    ERR("AV1 data_buffer parameter_size not matching vaapi parameter buffer size.");
                    return ROCDEC_RUNTIME_ERROR;
            }
#endif
            break;
        }

        default: {
            ERR("The codec type is not supported.");
            return ROCDEC_NOT_SUPPORTED;
        }
    }

    // Destroy the data buffers of the previous frame
    rocDecStatus rocdec_status = DestroyDataBuffers();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("Failed to destroy VAAPI buffer.");
        return rocdec_status;
    }

    CHECK_VAAPI(vaCreateBuffer(va_display_, va_context_id_, VAPictureParameterBufferType, pic_params_size, 1, pic_params_ptr, &pic_params_buf_id_));
    if (scaling_list_enabled) {
        CHECK_VAAPI(vaCreateBuffer(va_display_, va_context_id_, VAIQMatrixBufferType, iq_matrix_size, 1, iq_matrix_ptr, &iq_matrix_buf_id_));
    }
    // Resize if needed
    num_slices_ = pPicParams->num_slices;
    if (num_slices_ > slice_params_buf_id_.size()) {
        slice_params_buf_id_.resize(num_slices_, {0});
    }
    for (int i = 0; i < num_slices_; i++) {
        CHECK_VAAPI(vaCreateBuffer(va_display_, va_context_id_, VASliceParameterBufferType, slice_params_size, 1, slice_params_ptr, &slice_params_buf_id_[i]));
        slice_params_ptr = (void*)((uint8_t*)slice_params_ptr + slice_params_size);
    }
    CHECK_VAAPI(vaCreateBuffer(va_display_, va_context_id_, VASliceDataBufferType, pPicParams->bitstream_data_len, 1, (void*)pPicParams->bitstream_data, &slice_data_buf_id_));

    // Sumbmit buffers to VAAPI driver
    CHECK_VAAPI(vaBeginPicture(va_display_, va_context_id_, curr_surface_id));
    CHECK_VAAPI(vaRenderPicture(va_display_, va_context_id_, &pic_params_buf_id_, 1));
    if (scaling_list_enabled) {
        CHECK_VAAPI(vaRenderPicture(va_display_, va_context_id_, &iq_matrix_buf_id_, 1));
    }
    CHECK_VAAPI(vaRenderPicture(va_display_, va_context_id_, slice_params_buf_id_.data(), num_slices_));
    CHECK_VAAPI(vaRenderPicture(va_display_, va_context_id_, &slice_data_buf_id_, 1));
    CHECK_VAAPI(vaEndPicture(va_display_, va_context_id_));

    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::GetDecodeStatus(int pic_idx, RocdecDecodeStatus *decode_status) {
    VASurfaceStatus va_surface_status;
    if (pic_idx >= va_surface_ids_.size() || decode_status == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    CHECK_VAAPI(vaQuerySurfaceStatus(va_display_, va_surface_ids_[pic_idx], &va_surface_status));
    switch (va_surface_status) {
        case VASurfaceRendering:
            decode_status->decode_status = rocDecodeStatus_InProgress;
            break;
        case VASurfaceReady:
            decode_status->decode_status = rocDecodeStatus_Success;
            break;
        case VASurfaceDisplaying:
            decode_status->decode_status = rocDecodeStatus_Displaying;
            break;
        default:
           decode_status->decode_status = rocDecodeStatus_Invalid;
    }
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::ExportSurface(int pic_idx, VADRMPRIMESurfaceDescriptor &va_drm_prime_surface_desc) {
    if (pic_idx >= va_surface_ids_.size()) {
        return ROCDEC_INVALID_PARAMETER;
    }
    CHECK_VAAPI(vaExportSurfaceHandle(va_display_, va_surface_ids_[pic_idx],
                VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                VA_EXPORT_SURFACE_READ_ONLY |
                VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                &va_drm_prime_surface_desc));

   return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::ReconfigureDecoder(RocdecReconfigureDecoderInfo *reconfig_params) {
    if (reconfig_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    if (va_display_ == 0) {
        ERR("VAAPI decoder has not been initialized but reconfiguration of the decoder has been requested.");
        return ROCDEC_NOT_SUPPORTED;
    }
    CHECK_VAAPI(vaDestroySurfaces(va_display_, va_surface_ids_.data(), va_surface_ids_.size()));
    CHECK_VAAPI(vaDestroyContext(va_display_, va_context_id_));

    va_surface_ids_.clear();
    decoder_create_info_.width = reconfig_params->width;
    decoder_create_info_.height = reconfig_params->height;
    decoder_create_info_.num_decode_surfaces = reconfig_params->num_decode_surfaces;
    decoder_create_info_.target_height = reconfig_params->target_height;
    decoder_create_info_.target_width = reconfig_params->target_width;

    rocDecStatus rocdec_status = CreateSurfaces();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("Failed to create VAAPI surfaces during the decoder reconfiguration.");
        return rocdec_status;
    }
    rocdec_status = CreateContext();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("Failed to create a VAAPI context during the decoder reconfiguration.");
        return rocdec_status;
    }
    return rocdec_status;
}

rocDecStatus VaapiVideoDecoder::SyncSurface(int pic_idx) {
    if (pic_idx >= va_surface_ids_.size()) {
        return ROCDEC_INVALID_PARAMETER;
    }
    VASurfaceStatus surface_status;
    CHECK_VAAPI(vaQuerySurfaceStatus(va_display_, va_surface_ids_[pic_idx], &surface_status));
    if (surface_status != VASurfaceReady) {
        CHECK_VAAPI(vaSyncSurface(va_display_, va_surface_ids_[pic_idx]));
    }
    return ROCDEC_SUCCESS;
}