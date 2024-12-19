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
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include "../roc_decoder_caps.h"
#include "../../commons.h"
#include "../../../api/rocdecode.h"

#define CHECK_VAAPI(call) {\
    VAStatus va_status = call;\
    if (va_status != VA_STATUS_SUCCESS) {\
        std::cout << "VAAPI failure: " << #call << " failed with status: " << std::hex << "0x" << va_status << std::dec << " = '" << vaErrorStr(va_status) << "' at " <<  __FILE__ << ":" << __LINE__ << std::endl;\
        return ROCDEC_RUNTIME_ERROR;\
    }\
}

#define INIT_SLICE_PARAM_LIST_NUM 16 // initial slice parameter buffer list size

typedef enum {
    kSpx = 0, // Single Partition Accelerator
    kDpx = 1, // Dual Partition Accelerator
    kTpx = 2, // Triple Partition Accelerator
    kQpx = 3, // Quad Partition Accelerator
    kCpx = 4, // Core Partition Accelerator
} ComputePartition;

class VaapiVideoDecoder {
public:
    VaapiVideoDecoder(RocDecoderCreateInfo &decoder_create_info);
    ~VaapiVideoDecoder();
    rocDecStatus InitializeDecoder(std::string device_name, std::string gcn_arch_name, std::string& gpu_uuid);
    rocDecStatus SubmitDecode(RocdecPicParams *pPicParams);
    rocDecStatus GetDecodeStatus(int pic_idx, RocdecDecodeStatus* decode_status);
    rocDecStatus ExportSurface(int pic_idx, VADRMPRIMESurfaceDescriptor &va_drm_prime_surface_desc);
    rocDecStatus SyncSurface(int pic_idx);
    rocDecStatus ReconfigureDecoder(RocdecReconfigureDecoderInfo *reconfig_params);
private:
    RocDecoderCreateInfo decoder_create_info_;
    int drm_fd_;
    VADisplay va_display_;
    VAConfigAttrib va_config_attrib_;
    VAConfigID va_config_id_;
    VAProfile va_profile_;
    VAContextID va_context_id_;
    std::vector<VASurfaceID> va_surface_ids_;
    bool supports_modifiers_;

    VABufferID pic_params_buf_id_;
    VABufferID iq_matrix_buf_id_;
    std::vector<VABufferID> slice_params_buf_id_ = std::vector<VABufferID>(INIT_SLICE_PARAM_LIST_NUM, 0);
    uint32_t num_slices_;
    VABufferID slice_data_buf_id_;
    uint32_t slice_data_buf_size_;
    /**
     * @brief A map that associates GPU UUIDs with their corresponding render node indices.
     * 
     * This unordered map uses GPU UUIDs as keys (std::string) and maps them to their 
     * respective render node indices (int). It provides a fast lookup mechanism to 
     * retrieve the render node index for a given GPU UUID.
     */
    std::unordered_map<std::string, int> gpu_uuids_to_render_nodes_map_;

    rocDecStatus InitVAAPI(std::string drm_node);
    rocDecStatus CreateDecoderConfig();
    rocDecStatus CreateSurfaces();
    rocDecStatus CreateContext();
    rocDecStatus DestroyDataBuffers();
    void GetGpuUuids();
    void GetVisibleDevices(std::vector<int>& visible_devices);
    void GetCurrentComputePartition(std::vector<ComputePartition> &currnet_compute_partitions);
    void GetDrmNodeOffset(std::string device_name, uint8_t device_id, std::vector<int>& visible_devices,
                                    std::vector<ComputePartition> &current_compute_partitions, int &offset);
};