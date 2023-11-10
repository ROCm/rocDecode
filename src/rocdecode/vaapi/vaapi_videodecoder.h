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

#include <iostream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include "../roc_decoder_caps.h"
#include "../../commons.h"
#include "../../../api/rocdecode.h"

#define DEFAULT_SLICE_DATA_BUF_SIZE 2 * 1024 * 1024  // 2 MB

#define CHECK_VAAPI(call) {                                               \
    VAStatus va_status = (call);                                          \
    if (va_status != VA_STATUS_SUCCESS) {                                 \
        std::cout << "VAAPI failure: 'status#" << va_status << "' at " <<  __FILE__ << ":" << __LINE__ << std::endl;\
        return ROCDEC_RUNTIME_ERROR;                                      \
    }                                                                     \
}

class VaapiVideoDecoder {
public:
    VaapiVideoDecoder(RocDecoderCreateInfo &decoder_create_info);
    ~VaapiVideoDecoder();
    rocDecStatus InitializeDecoder(std::string gcn_arch_name);
    rocDecStatus SubmitDecode(RocdecPicParams *pPicParams);
    rocDecStatus GetDecodeStatus(int pic_idx, RocdecDecodeStatus* decode_status);
    rocDecStatus ExportSurface(int pic_idx, VADRMPRIMESurfaceDescriptor &va_drm_prime_surface_desc);

private:
    RocDecoderCreateInfo decoder_create_info_;
    int drm_fd_;
    VADisplay va_display_;
    VAConfigAttrib va_config_attrib_;
    VAConfigID va_config_id_;
    VAProfile va_profile_;
    VAContextID va_context_id_;
    std::vector<VASurfaceID> va_surface_ids_;

    VABufferID pic_params_buf_id_;
    VABufferID iq_matrix_buf_id_;
    VABufferID slice_params_buf_id_;
    VABufferID slice_data_buf_id_;
    uint32_t slice_data_buf_size_;

    rocDecStatus InitVAAPI(std::string drm_node);
    rocDecStatus CreateDecoderConfig();
    rocDecStatus CreateSurfaces();
    rocDecStatus CreateContext();
    rocDecStatus CreateDataBuffers();
    rocDecStatus DestroyDataBuffers();
};