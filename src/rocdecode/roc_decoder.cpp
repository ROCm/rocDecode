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

#include "../commons.h"
#include "roc_decoder.h"

RocDecoder::RocDecoder(RocDecoderCreateInfo& decoder_create_info): va_video_decoder_{decoder_create_info}, decoder_create_info_{decoder_create_info} {}

 RocDecoder::~RocDecoder() {}

 rocDecStatus RocDecoder::InitializeDecoder() {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;
    rocdec_status = InitHIP(decoder_create_info_.deviceid);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to initilize the HIP! with rocDecStatus# " + TOSTR(rocdec_status));
        return rocdec_status;
    }
    if (decoder_create_info_.ulNumDecodeSurfaces < 1) {
        ERR("ERROR: invalid number of decode surfaces ");
        return ROCDEC_INVALID_PARAMETER;
    }
    hip_ext_mem_.resize(decoder_create_info_.ulNumDecodeSurfaces);

    rocdec_status = va_video_decoder_.InitializeDecoder(hip_dev_prop_.gcnArchName);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to initilize the VAAPI Video decoder! with rocDecStatus# " + TOSTR(rocdec_status));
        return rocdec_status;
    }

     return rocdec_status;
 }

rocDecStatus RocDecoder::DecodeFrame(RocdecPicParams *pic_params) {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;
    rocdec_status = va_video_decoder_.SubmitDecode(pic_params);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Decode submission is not successful! with rocDecStatus# " + TOSTR(rocdec_status));
    }

     return rocdec_status;
}

rocDecStatus RocDecoder::GetDecodeStatus(int pic_idx, RocdecDecodeStatus* decode_status) {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;
    rocdec_status = va_video_decoder_.GetDecodeStatus(pic_idx, decode_status);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to query the decode status! with rocDecStatus# " + TOSTR(rocdec_status));
    }
    return rocdec_status;
}

rocDecStatus RocDecoder::ReconfigureDecoder(RocdecReconfigureDecoderInfo *reconfig_params) {
    if (reconfig_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    rocDecStatus rocdec_status = va_video_decoder_.ReconfigureDecoder(reconfig_params);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Reconfiguration of the decoder failed with rocDecStatus# " + TOSTR(rocdec_status));
        return rocdec_status;
    }
    return rocdec_status;
}

rocDecStatus RocDecoder::MapVideoFrame(int pic_idx, void *dev_mem_ptr[3], uint32_t horizontal_pitch[3], RocdecProcParams *vid_postproc_params) {
    if (pic_idx >= hip_ext_mem_.size() || &dev_mem_ptr[0] == nullptr || vid_postproc_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;
    hipExternalMemoryHandleDesc external_mem_handle_desc_ = {};
    hipExternalMemoryBufferDesc external_mem_buffer_desc_ = {};
    VADRMPRIMESurfaceDescriptor va_drm_prime_surface_desc = {};

    rocdec_status = va_video_decoder_.ExportSurface(pic_idx, va_drm_prime_surface_desc);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to export surface for picture id" + TOSTR(pic_idx) + " , with rocDecStatus# " + TOSTR(rocdec_status));
        return rocdec_status;
    }

    external_mem_handle_desc_.type = hipExternalMemoryHandleTypeOpaqueFd;
    external_mem_handle_desc_.handle.fd = va_drm_prime_surface_desc.objects[0].fd;
    external_mem_handle_desc_.size = va_drm_prime_surface_desc.objects[0].size;
    CHECK_HIP(hipImportExternalMemory(&hip_ext_mem_[pic_idx], &external_mem_handle_desc_));

    external_mem_buffer_desc_.size = va_drm_prime_surface_desc.objects[0].size;
    CHECK_HIP(hipExternalMemoryGetMappedBuffer(&*&dev_mem_ptr[0], hip_ext_mem_[pic_idx], &external_mem_buffer_desc_));
    horizontal_pitch[0] = va_drm_prime_surface_desc.layers[0].pitch[0];
    if (va_drm_prime_surface_desc.num_layers == 2) {
        *&dev_mem_ptr[1] = static_cast<uint8_t*>(*&dev_mem_ptr[0]) + va_drm_prime_surface_desc.layers[1].offset[0];
        horizontal_pitch[1] = va_drm_prime_surface_desc.layers[1].pitch[0];
    } else if (va_drm_prime_surface_desc.num_layers == 3) {
        *&dev_mem_ptr[2] = static_cast<uint8_t*>(*&dev_mem_ptr[0]) + va_drm_prime_surface_desc.layers[2].offset[0];
        horizontal_pitch[2] = va_drm_prime_surface_desc.layers[2].pitch[0];
    }

    for (auto i = 0; i < va_drm_prime_surface_desc.num_objects; ++i) {
        close(va_drm_prime_surface_desc.objects[i].fd);
    }

    return rocdec_status;
}

rocDecStatus RocDecoder::UnMapVideoFrame(int pic_idx) {
    if (pic_idx >= hip_ext_mem_.size()) {
        return ROCDEC_INVALID_PARAMETER;
    }

    CHECK_HIP(hipDestroyExternalMemory(hip_ext_mem_[pic_idx]));

    return ROCDEC_SUCCESS;
}


rocDecStatus RocDecoder::InitHIP(int device_id) {
    CHECK_HIP(hipGetDeviceCount(&num_devices_));
    if (num_devices_ < 1) {
        ERR("ERROR: didn't find any GPU!");
        return ROCDEC_DEVICE_INVALID;
    }
    CHECK_HIP(hipSetDevice(device_id));
    CHECK_HIP(hipGetDeviceProperties(&hip_dev_prop_, device_id));

    return ROCDEC_SUCCESS;
}
