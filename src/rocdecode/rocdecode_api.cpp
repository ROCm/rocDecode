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
#include "dec_handle.h"
#include "rocdecode.h"
#include "roc_decoder_caps.h"
#include "../commons.h"


/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecCreateDecoder(rocDecDecoderHandle *decoder_handle, RocDecoderCreateInfo *decoder_create_info)
//! Create the decoder object based on decoder_create_info. A handle to the created decoder is returned
/*****************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecCreateDecoder(rocDecDecoderHandle *decoder_handle, RocDecoderCreateInfo *decoder_create_info) {
    if (decoder_handle == nullptr || decoder_create_info == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    rocDecDecoderHandle handle = nullptr;
    try {
        handle = new DecHandle(*decoder_create_info);
    }
    catch(const std::exception& e) {
        ERR( STR("Failed to init the rocDecode handle, ") + STR(e.what()))
        return ROCDEC_NOT_INITIALIZED;
    }
    *decoder_handle = handle;
    return static_cast<DecHandle *>(handle)->roc_decoder_->InitializeDecoder();
}

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDestroyDecoder(rocDecDecoderHandle decoder_handle)
//! Destroy the decoder object
/*****************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecDestroyDecoder(rocDecDecoderHandle decoder_handle) {
    if (decoder_handle == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    auto handle = static_cast<DecHandle *>(decoder_handle);
    delete handle;
    return ROCDEC_SUCCESS;
}

/**********************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocdecGetDecoderCaps(rocDecDecoderHandle decoder_handle, RocdecDecodeCaps *pdc)
//! Queries decode capabilities of AMD's VCN decoder based on codec type, chroma_format and BitDepthMinus8 parameters.
//! 1. Application fills IN parameters codec_type, chroma_format and BitDepthMinus8 of RocdecDecodeCaps structure
//! 2. On calling rocdecGetDecoderCaps, driver fills OUT parameters if the IN parameters are supported
//!    If IN parameters passed to the driver are not supported by AMD-VCN-HW, then all OUT params are set to 0.
/**********************************************************************************************************************/
rocDecStatus ROCDECAPI
rocDecGetDecoderCaps(RocdecDecodeCaps *pdc) {
    if (pdc == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    hipError_t hip_status = hipSuccess;
    int num_devices = 0;
    hipDeviceProp_t hip_dev_prop;
    hip_status = hipGetDeviceCount(&num_devices);
    if (hip_status != hipSuccess) {
        ERR("ERROR: hipGetDeviceCount failed!" + TOSTR(hip_status));
        return ROCDEC_DEVICE_INVALID;
    }
    if (num_devices < 1) {
        ERR("ERROR: didn't find any GPU!");
        return ROCDEC_DEVICE_INVALID;
    }
    if (pdc->device_id >= num_devices) {
        ERR("ERROR: the requested device_id is not found! ");
        return ROCDEC_DEVICE_INVALID;
    }
    hip_status = hipGetDeviceProperties(&hip_dev_prop, pdc->device_id);
    if (hip_status != hipSuccess) {
        ERR("ERROR: hipGetDeviceProperties for device (" +TOSTR(pdc->device_id) + " ) failed! (" + TOSTR(hip_status) + ")" );
        return ROCDEC_DEVICE_INVALID;
    }

    RocDecVcnCodecSpec& vcn_codec_spec = RocDecVcnCodecSpec::GetInstance();
    return vcn_codec_spec.GetDecoderCaps(pdc);
}

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDecodeFrame(rocDecDecoderHandle decoder_handle, RocdecPicParams *pic_params)
//! Decodes a single picture
//! Submits the frame for HW decoding 
/*****************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecDecodeFrame(rocDecDecoderHandle decoder_handle, RocdecPicParams *pic_params) {
    if (decoder_handle == nullptr || pic_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    auto handle = static_cast<DecHandle *>(decoder_handle);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder_->DecodeFrame(pic_params);
    }
    catch(const std::exception& e) {
        handle->CaptureError(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI RocdecGetDecodeStatus(rocDecDecoderHandle decoder_handle, int pic_idx, RocdecDecodeStatus* decode_status);
//! Get the decode status for frame corresponding to pic_idx
//! API is currently supported for HEVC codec.
//! API returns CUDA_ERROR_NOT_SUPPORTED error code for unsupported GPU or codec.
/************************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecGetDecodeStatus(rocDecDecoderHandle decoder_handle, int pic_idx, RocdecDecodeStatus* decode_status) {
    if (decoder_handle == nullptr || decode_status == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    auto handle = static_cast<DecHandle *>(decoder_handle);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder_->GetDecodeStatus(pic_idx, decode_status);
    }
    catch(const std::exception& e) {
        handle->CaptureError(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/*********************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle decoder_handle, RocdecReconfigureDecoderInfo *reconfig_params)
//! Used to reuse single decoder for multiple clips. Currently supports resolution change, resize params
//! params, target area params change for same codec. Must be called during RocdecParserParams::pfn_sequence_callback
/*********************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecReconfigureDecoder(rocDecDecoderHandle decoder_handle, RocdecReconfigureDecoderInfo *reconfig_params) {
    if (decoder_handle == nullptr || reconfig_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    auto handle = static_cast<DecHandle *>(decoder_handle);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder_->ReconfigureDecoder(reconfig_params);
    }
    catch(const std::exception& e) {
        handle->CaptureError(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/************************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecGetVideoFrame(rocDecDecoderHandle decoder_handle, int pic_idx, unsigned int *dev_mem_ptr,
//!         unsigned int *horizontal_pitch, RocdecProcParams *vid_postproc_params);
//! Post-process and map video frame corresponding to pic_idx for use in HIP. Returns HIP device pointer and associated
//! pitch(horizontal stride) of the video frame. Returns device memory pointers for each plane (Y, U and V) seperately
/************************************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecGetVideoFrame(rocDecDecoderHandle decoder_handle, int pic_idx,
                    void *dev_mem_ptr[3], uint32_t (&horizontal_pitch)[3], RocdecProcParams *vid_postproc_params) {
    if (decoder_handle == nullptr || dev_mem_ptr == nullptr || horizontal_pitch == nullptr || vid_postproc_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    auto handle = static_cast<DecHandle *>(decoder_handle);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder_->GetVideoFrame(pic_idx, dev_mem_ptr, horizontal_pitch, vid_postproc_params);
    }
    catch(const std::exception& e) {
        handle->CaptureError(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/*****************************************************************************************************/
//! \fn const char* ROCDECAPI rocDecGetErrorName(rocDecStatus rocdec_status)
//! \ingroup group_amd_rocdecode
//! Return name of the specified error code in text form.
/*****************************************************************************************************/
const char* ROCDECAPI rocDecGetErrorName(rocDecStatus rocdec_status) {
    switch (rocdec_status) {
        case ROCDEC_DEVICE_INVALID:
            return "ROCDEC_DEVICE_INVALID";
        case ROCDEC_CONTEXT_INVALID:
            return "ROCDEC_CONTEXT_INVALID";
        case ROCDEC_RUNTIME_ERROR:
            return "ROCDEC_RUNTIME_ERROR";
        case ROCDEC_OUTOF_MEMORY:
            return "ROCDEC_OUTOF_MEMORY";
        case ROCDEC_INVALID_PARAMETER:
            return "ROCDEC_INVALID_PARAMETER";
        case ROCDEC_NOT_IMPLEMENTED:
            return "ROCDEC_NOT_IMPLEMENTED";
        case ROCDEC_NOT_INITIALIZED:
            return "ROCDEC_NOT_INITIALIZED";
        case ROCDEC_NOT_SUPPORTED:
            return "ROCDEC_NOT_SUPPORTED";
        default:
            return "UNKNOWN_ERROR";
    }
}