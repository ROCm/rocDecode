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

#pragma once

#include <assert.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <string.h>
#include <map>
#include "../api/rocdecode.h"
#include <hip/hip_runtime.h>
#include "vaapi/vaapi_videodecoder.h"

#define CHECK_HIP(call) {\
    hipError_t hip_status = call;\
    if (hip_status != hipSuccess) {\
        std::cout << "HIP failure: " << #call << " failed with 'status# " << hip_status << "' at " <<  __FILE__ << ":" << __LINE__ << std::endl;\
        return ROCDEC_RUNTIME_ERROR;\
    }\
}

class RocDecoder {
public:
    RocDecoder(RocDecoderCreateInfo &decoder_create_info);
    ~RocDecoder();
    rocDecStatus InitializeDecoder();
    rocDecStatus DecodeFrame(RocdecPicParams *pic_params);
    rocDecStatus GetDecodeStatus(int pic_idx, RocdecDecodeStatus* decode_status);
    rocDecStatus ReconfigureDecoder(RocdecReconfigureDecoderInfo *reconfig_params);
    rocDecStatus MapVideoFrame(int pic_idx, void *dev_mem_ptr[3], uint32_t horizontal_pitch[3], RocdecProcParams *vid_postproc_params);
    rocDecStatus UnMapVideoFrame(int pic_idx);

private:
    rocDecStatus InitHIP(int device_id);
    int num_devices_;
    RocDecoderCreateInfo decoder_create_info_;
    VaapiVideoDecoder va_video_decoder_;
    hipDeviceProp_t hip_dev_prop_;
    std::vector<hipExternalMemory_t> hip_ext_mem_;
};