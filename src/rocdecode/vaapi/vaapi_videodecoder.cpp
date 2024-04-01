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

rocDecStatus VaapiVideoDecoder::InitializeDecoder(std::string device_name, std::string gcn_arch_name) {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;

    //Before initializing the VAAPI, first check to see if the requested codec config is supported
    RocDecVcnCodecSpec& vcn_codec_spec = RocDecVcnCodecSpec::GetInstance();
    if (!vcn_codec_spec.IsCodecConfigSupported(gcn_arch_name, decoder_create_info_.codec_type, decoder_create_info_.chroma_format,
        decoder_create_info_.bit_depth_minus_8, decoder_create_info_.output_format)) {
        ERR("The codec config combination is not supported.");
        return ROCDEC_NOT_SUPPORTED;
    }

    std::size_t pos = gcn_arch_name.find_first_of(":");
    std::string gcn_arch_name_base = (pos != std::string::npos) ? gcn_arch_name.substr(0, pos) : gcn_arch_name;

    std::vector<int> visible_devices;
    GetVisibleDevices(visible_devices);

    int num_render_cards_per_socket = 1;
    int offset = 0;
    if (gcn_arch_name_base.compare("gfx940") == 0 ||
        gcn_arch_name_base.compare("gfx941") == 0 ||
        gcn_arch_name_base.compare("gfx942") == 0) {
            std::vector<ComputePartition> current_compute_partitions;
            GetCurrentComputePartition(current_compute_partitions);
            if (current_compute_partitions.empty()) {
                //if the current_compute_partitions is empty then the default SPX mode is assumed.
                num_render_cards_per_socket = 8;
            } else {
                GetNumRenderCardsPerDevice(device_name, decoder_create_info_.device_id, visible_devices, current_compute_partitions, num_render_cards_per_socket, offset);
            }
        }

    std::string drm_node = "/dev/dri/renderD";
    if (decoder_create_info_.device_id < visible_devices.size()) {
        drm_node += std::to_string(128 + offset + visible_devices[decoder_create_info_.device_id] * num_render_cards_per_socket);
    } else {
        drm_node += std::to_string(128 + offset + decoder_create_info_.device_id * num_render_cards_per_socket);
    }
    rocdec_status = InitVAAPI(drm_node);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("Failed to initilize the VAAPI.");
        return rocdec_status;
    }
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

rocDecStatus VaapiVideoDecoder::InitVAAPI(std::string drm_node) {
    drm_fd_ = open(drm_node.c_str(), O_RDWR);
    if (drm_fd_ < 0) {
        ERR("Failed to open drm node." + drm_node);
        return ROCDEC_NOT_INITIALIZED;
    }
    va_display_ = vaGetDisplayDRM(drm_fd_);
    if (!va_display_) {
        ERR("Failed to create va_display.");
        return ROCDEC_NOT_INITIALIZED;
    }
    vaSetInfoCallback(va_display_, NULL, NULL);
    int major_version = 0, minor_version = 0;
    CHECK_VAAPI(vaInitialize(va_display_, &major_version, &minor_version));
    return ROCDEC_SUCCESS;
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
    uint8_t surface_format;
    switch (decoder_create_info_.chroma_format) {
        case rocDecVideoChromaFormat_Monochrome:
            surface_format = VA_RT_FORMAT_YUV400;
            break;
        case rocDecVideoChromaFormat_420:
            surface_format = VA_RT_FORMAT_YUV420;
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
        decoder_create_info_.height, va_surface_ids_.data(), va_surface_ids_.size(), nullptr, 0));

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
    while (surface_status != VASurfaceReady) {
        VAStatus va_status = vaSyncSurface(va_display_, va_surface_ids_[pic_idx]);
        /* Current implementation of vaSyncSurface() does not block indefinitely (contrary to VA-API spec), it returns
         * VA_STATUS_ERROR_TIMEDOUT error when it blocks for a certain amount of time. Although time out can come from
         * various reasons, we treat it as non-fatal and contiue waiting.
         */
        if (va_status != VA_STATUS_SUCCESS) {
            if (va_status == 0x26 /*VA_STATUS_ERROR_TIMEDOUT*/) {
                CHECK_VAAPI(vaQuerySurfaceStatus(va_display_, va_surface_ids_[pic_idx], &surface_status));
            } else {
                std::cout << "vaSyncSurface() failed with error code: 0x" << std::hex << va_status << std::dec << "', status: " << vaErrorStr(va_status) << "' at " <<  __FILE__ << ":" << __LINE__ << std::endl;
                return ROCDEC_RUNTIME_ERROR;
            }
        } else {
            break;
        }
    }
    return ROCDEC_SUCCESS;
}

void VaapiVideoDecoder::GetVisibleDevices(std::vector<int>& visible_devices_vetor) {
    char *visible_devices = std::getenv("HIP_VISIBLE_DEVICES");
    if (visible_devices != nullptr) {
        char *token = std::strtok(visible_devices,",");
        while (token != nullptr) {
            visible_devices_vetor.push_back(std::atoi(token));
            token = std::strtok(nullptr,",");
        }
    std::sort(visible_devices_vetor.begin(), visible_devices_vetor.end());
    }
}

void VaapiVideoDecoder::GetCurrentComputePartition(std::vector<ComputePartition> &current_compute_partitions) {
    std::string search_path = "/sys/devices/";
    std::string partition_file = "current_compute_partition";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(search_path)) {
        if (entry.path().filename() == partition_file) {
            std::ifstream file(entry.path());
            if (file.is_open()) {
                std::string partition;
                std::getline(file, partition);
                if (partition.compare("SPX") == 0 || partition.compare("spx") == 0) {
                    current_compute_partitions.push_back(kSpx);
                } else if (partition.compare("DPX") == 0 || partition.compare("dpx") == 0) {
                    current_compute_partitions.push_back(kDpx);
                } else if (partition.compare("TPX") == 0 || partition.compare("tpx") == 0) {
                    current_compute_partitions.push_back(kTpx);
                } else if (partition.compare("QPX") == 0 || partition.compare("qpx") == 0) {
                    current_compute_partitions.push_back(kQpx);
                } else if (partition.compare("CPX") == 0 || partition.compare("cpx") == 0) {
                    current_compute_partitions.push_back(kCpx);
                }
                file.close();
            }
        }
    }
}

void VaapiVideoDecoder::GetNumRenderCardsPerDevice(std::string device_name, uint8_t device_id, std::vector<int>& visible_devices,
                                                   std::vector<ComputePartition> &current_compute_partitions,
                                                   int &num_render_cards_per_socket, int &offset) {
    offset = 0;
    if (!current_compute_partitions.empty()) {
        switch (current_compute_partitions[0]) {
            case kSpx:
                num_render_cards_per_socket = 8;
                break;
            case kDpx:
                num_render_cards_per_socket = 4;
                break;
            case kTpx:
                num_render_cards_per_socket = 2;
                // Please note that although there are only 6 XCCs per socket on MI300A,
                // there are two dummy render nodes added by the driver.
                // This needs to be taken into account when creating drm_node on each socket in TPX mode.
                if (device_id < visible_devices.size()) {
                    offset = (visible_devices[device_id] / 3) * 2;
                } else {
                    offset = (device_id / 3) * 2;
                }
                break;
            case kQpx:
                num_render_cards_per_socket = 2;
                break;
            case kCpx:
                num_render_cards_per_socket = 1;
                // Please note that both MI300A and MI300X have the same gfx_arch_name which is
                // gfx942. Therefore we cannot use the gfx942 to identify MI300A.
                // instead use the device name and look for MI300A
                // Also, as explained aboe in the TPX mode section, we need to be taken into account
                // the extra two dummy nodes when creating drm_node on each socket in CPX mode as well.
                std::string mi300a = "MI300A";
                size_t found_mi300a = device_name.find(mi300a);
                if (found_mi300a != std::string::npos) {
                    offset = (device_id / 6) * 2;
                }
                break;
        }
    }
}