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

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <mutex>
#include <algorithm>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif
#include <libdrm/amdgpu_drm.h>
#include <libdrm/amdgpu.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include "../../commons.h"
#include "../../../api/rocdecode.h"
#include "rocdecode_va_context.h"

#define INIT_SLICE_PARAM_LIST_NUM 16 // initial slice parameter buffer list size

class VaapiVideoDecoder {
public:
    VaapiVideoDecoder(RocDecoderCreateInfo &decoder_create_info);
    ~VaapiVideoDecoder();
    rocDecStatus InitializeDecoder(std::string device_name, std::string gcn_arch_name);
    rocDecStatus SubmitDecode(RocdecPicParams *pPicParams);
    rocDecStatus GetDecodeStatus(int pic_idx, RocdecDecodeStatus* decode_status);
    rocDecStatus ExportSurface(int pic_idx, VADRMPRIMESurfaceDescriptor &va_drm_prime_surface_desc);
    rocDecStatus SyncSurface(int pic_idx);
    rocDecStatus ReconfigureDecoder(RocdecReconfigureDecoderInfo *reconfig_params);

private:
    RocDecoderCreateInfo decoder_create_info_;
    int drm_fd_;
    VADisplay va_display_;
    VAProfile va_profile_;
    VAConfigAttrib va_config_attrib_;
    VAConfigID va_config_id_;
    VAContextID va_context_id_;
    std::vector<VASurfaceID> va_surface_ids_;
    bool supports_modifiers_;

    VABufferID pic_params_buf_id_;
    VABufferID iq_matrix_buf_id_;
    std::vector<VABufferID> slice_params_buf_id_ = std::vector<VABufferID>(INIT_SLICE_PARAM_LIST_NUM, 0);
    uint32_t num_slices_;
    VABufferID slice_data_buf_id_;
    uint32_t slice_data_buf_size_;

    bool IsCodecConfigSupported(int device_id, rocDecVideoCodec codec_type, rocDecVideoChromaFormat chroma_format, uint32_t bit_depth_minus8, rocDecVideoSurfaceFormat output_format);
    rocDecStatus CreateDecoderConfig();
    rocDecStatus CreateSurfaces();
    rocDecStatus CreateContext();
    rocDecStatus DestroyDataBuffers();
};