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


class RocDecoder {
public:
    RocDecoder(int device_id = 0);
    ~RocDecoder();
    rocDecStatus getDecoderCaps(RocdecDecodeCaps *pdc);
    rocDecStatus decodeFrame(RocdecPicParams *pPicParams);
    rocDecStatus getDecodeStatus(int nPicIdx, RocdecDecodeStatus* pDecodeStatus);
    rocDecStatus reconfigureDecoder(RocdecReconfigureDecoderInfo *pDecReconfigParams);
    rocDecStatus mapVideoFrame(int nPicIdx, void *pDevMemPtr[3], unsigned int *pHorizontalPitch[3], RocdecProcParams *pVidPostprocParams);
    rocDecStatus unMapVideoFrame(void *pMappedDevPtr);

private:
    rocDecStatus initHIP(int device_id);
    void initDRMnodes();

    int num_devices_;
    int device_id_;
    hipDeviceProp_t hip_dev_prop_;
    hipStream_t hip_stream_;
    std::vector<std::string> drm_nodes_;
};
