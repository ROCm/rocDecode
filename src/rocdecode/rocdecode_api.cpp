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
#include "dec_handle.h"
#include "rocdecode.h"
#include "../commons.h"

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecCreateDecoder(rocDecDecoderHandle *phDecoder, RocdecDecoderCreateInfo *pdci)
//! Create the decoder object based on pdci. A handle to the created decoder is returned
/*****************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecCreateDecoder(rocDecDecoderHandle *phDecoder, RocdecDecoderCreateInfo *pdci) {
    rocDecDecoderHandle handle = nullptr;
    try {
        handle = new DecHandle();
    } 
    catch(const std::exception& e) {
        ERR( STR("Failed to init the rocDecode handle, ") + STR(e.what()))
    }
    *phDecoder = handle;
    return ROCDEC_SUCCESS;
}

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDestroyDecoder(rocDecDecoderHandle hDecoder)
//! Destroy the decoder object
/*****************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecDestroyDecoder(rocDecDecoderHandle hDecoder) {
    auto handle = static_cast<DecHandle *> (hDecoder);
    delete handle;
    return ROCDEC_SUCCESS;
}

/**********************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocdecGetDecoderCaps(rocDecDecoderHandle hDecoder, RocdecDecodeCaps *pdc)
//! Queries decode capabilities of AMD's VCN decoder based on CodecType, ChromaFormat and BitDepthMinus8 parameters.
//! 1. Application fills IN parameters CodecType, ChromaFormat and BitDepthMinus8 of RocdecDecodeCaps structure
//! 2. On calling rocdecGetDecoderCaps, driver fills OUT parameters if the IN parameters are supported
//!    If IN parameters passed to the driver are not supported by AMD-VCN-HW, then all OUT params are set to 0.
/**********************************************************************************************************************/
rocDecStatus ROCDECAPI
rocDecGetDecoderCaps(RocdecDecodeCaps *pdc) {
    return ROCDEC_NOT_IMPLEMENTED;
}

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDecodeFrame(rocDecDecoderHandle hDecoder, RocdecPicParams *pPicParams)
//! Decodes a single picture
//! Submits the frame for HW decoding 
/*****************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecDecodeFrame(rocDecDecoderHandle hDecoder, RocdecPicParams *pPicParams) {
    auto handle = static_cast<DecHandle *> (hDecoder);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder->decodeFrame(pPicParams);
    }
    catch(const std::exception& e) {
        handle->capture_error(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI RocdecGetDecodeStatus(rocDecDecoderHandle hDecoder, int nPicIdx, RocdecDecodeStatus* pDecodeStatus);
//! Get the decode status for frame corresponding to nPicIdx
//! API is currently supported for HEVC, H264 and JPEG codecs.
//! API returns CUDA_ERROR_NOT_SUPPORTED error code for unsupported GPU or codec.
/************************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecGetDecodeStatus(rocDecDecoderHandle hDecoder, int nPicIdx, RocdecDecodeStatus* pDecodeStatus) {
    auto handle = static_cast<DecHandle *> (hDecoder);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder->getDecodeStatus(nPicIdx, pDecodeStatus);
    }
    catch(const std::exception& e) {
        handle->capture_error(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/*********************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, RocdecReconfigureDecoderInfo *pDecReconfigParams)
//! Used to reuse single decoder for multiple clips. Currently supports resolution change, resize params 
//! params, target area params change for same codec. Must be called during RocdecParserParams::pfnSequenceCallback 
/*********************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, RocdecReconfigureDecoderInfo *pDecReconfigParams) {
    auto handle = static_cast<DecHandle *> (hDecoder);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder->reconfigureDecoder(pDecReconfigParams);
    }
    catch(const std::exception& e) {
        handle->capture_error(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/************************************************************************************************************************/
//! \fn extern rocDecStatus ROCDECAPI rocDecMapVideoFrame(rocDecDecoderHandle hDecoder, int nPicIdx,
//!                                           unsigned int *pDevMemPtr, unsigned int *pHorizontalPitch,
//!                                           RocdecProcParams *pVidPostprocParams);
//! Post-process and map video frame corresponding to nPicIdx for use in HIP. Returns HIP device pointer and associated
//! pitch(horizontal stride) of the video frame. Returns device memory pointers for each plane (Y, U and V) seperately
/************************************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecMapVideoFrame(rocDecDecoderHandle hDecoder, int nPicIdx,
                    void *pDevMemPtr[3], uint32_t (&pHorizontalPitch)[3], RocdecProcParams *pVidPostprocParams) {
    auto handle = static_cast<DecHandle *> (hDecoder);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder->mapVideoFrame(nPicIdx, pDevMemPtr, pHorizontalPitch, pVidPostprocParams);
    }
    catch(const std::exception& e) {
        handle->capture_error(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecUnMapVideoFrame(rocDecDecoderHandle hDecoder, void *pMappedDevPtr)
//! Unmap a previously mapped video frame with the associated mapped raw pointer (pMappedDevPtr) 
/*****************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecUnMapVideoFrame(rocDecDecoderHandle hDecoder, void *pMappedDevPtr) {
    auto handle = static_cast<DecHandle *> (hDecoder);
    rocDecStatus ret;
    try {
        ret = handle->roc_decoder->unMapVideoFrame(pMappedDevPtr);
    }
    catch(const std::exception& e) {
        handle->capture_error(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}
